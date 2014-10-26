/*
 * timer.c - timer event infrastructure
 *
 * The design is inspired by "Hashed and Hierarchical Timing Wheels: Data
 * Structures for the Efficient Implementation of a Timer Facility" by
 * George Varghese and Tony Lauck. SOSP 87.
 *
 * Specificially, we use Scheme 7 described in the paper, where
 * hierarchical sets of buckets are used.
 */

#undef DEBUG_TIMER

#include <ix/timer.h>
#include <ix/errno.h>
#include <ix/log.h>
#include <ix/cpu.h>
#include <ix/kstats.h>
#include <ix/ethfg.h>
#include <assert.h>
#include <time.h>
#include <ix/log.h>

#define WHEEL_SHIFT_LOG2	3
#define WHEEL_SHIFT		(1 << WHEEL_SHIFT_LOG2)
#define WHEEL_SIZE		(1 << WHEEL_SHIFT)
#define WHEEL_MASK		(WHEEL_SIZE - 1)
#define WHEEL_COUNT		3

#define MIN_DELAY_SHIFT		4
#define MIN_DELAY_US		(1 << MIN_DELAY_SHIFT)
#define MIN_DELAY_MASK		(MIN_DELAY_US - 1)
#define MAX_DELAY_US \
	(MIN_DELAY_US * (1 << (WHEEL_COUNT * WHEEL_SHIFT)))

#define WHEEL_IDX_TO_SHIFT(idx) \
	((idx) * WHEEL_SHIFT + MIN_DELAY_SHIFT)
#define WHEEL_OFFSET(val, idx) \
	(((val) >> WHEEL_IDX_TO_SHIFT(idx)) & WHEEL_MASK)

/*
 * NOTE: these parameters may need to be tweaked.
 *
 * Right now we have the following wheels:
 *
 * high precision wheel: 256 x 16 us increments
 * medium precision wheel: 256 x 4 ms increments
 * low precision wheel: 256 x 1 second increments
 *
 * Total range 0 to 256 seconds...
 */

struct timerwheel {
	struct hlist_head wheels[WHEEL_COUNT][WHEEL_SIZE];
	uint64_t wheelhint_mask;  // one hot encoding
};

static DEFINE_PERFG(struct timerwheel,timer_wheel_fg);
static DEFINE_PERCPU(struct timerwheel, timer_wheel_cpu);

static DEFINE_PERCPU(uint64_t, now_us);
static DEFINE_PERCPU(uint64_t, timer_pos);

static struct timerwheel *fg_timerwheel[ETH_MAX_TOTAL_FG];

/*
 * bitvector with possible non-empty bucket
 * kept on a per-cpu basis to avoid false sharing
 */

#define WHEELHINT_SHARING (ETH_MAX_TOTAL_FG/64)
struct wheel0fg_hint { 
	uint64_t hints[WHEEL_SIZE];
};

static DEFINE_PERCPU(struct wheel0fg_hint, wheel0fg_hint);



int cycles_per_us __aligned(64);

/**
 * __timer_delay_us - spins the CPU for the specified delay
 * @us: the delay in microseconds
 */
void __timer_delay_us(uint64_t us)
{
	uint64_t cycles = us * cycles_per_us;
	unsigned long start = rdtsc();

	while (rdtsc() - start < cycles)
		cpu_relax();	
}

static inline bool timer_expired(struct timerwheel *tw, struct timer *t)
{
	return (t->expires <= percpu_get(now_us));
}


static void timer_insert(struct timerwheel *tw, struct timer *t,
			 uint64_t left, uint64_t expires)
{
	struct hlist_head *bucket;
	int index = ((63 - clz64(left | MIN_DELAY_US) - MIN_DELAY_SHIFT)
			>> WHEEL_SHIFT_LOG2);
	assert (index >=0 && index <=2);
	int offset = WHEEL_OFFSET(expires, index);

	if (index ==2) {
		uint64_t prev = expires-left;
		assert (offset != WHEEL_OFFSET(prev,2));
	}

	bucket =&tw->wheels[index][offset];
	hlist_add_head(bucket, &t->link);
	if (index==0) {
		struct wheel0fg_hint *hint = &percpu_get(wheel0fg_hint);
		assert(tw->wheelhint_mask);
		hint->hints[offset] |= tw->wheelhint_mask;
	}
	//printf("timer_insert %d:%d expires %lu now:%lu left:%lu\n",index,offset,expires,percpu_get(now_us),left);

}

/**
 * timer_add - adds a timer
 * @l: the timer
 * @usecs: the time interval from present to fire the timer
 *
 * Returns 0 if successful, otherwise failure.
 */
static int __timer_add(struct timerwheel *tw, struct timer *t, uint64_t usecs)
{
	uint64_t expires;
	assert(usecs>0);
	if (unlikely(usecs >= MAX_DELAY_US)) {
		panic("__timer_add out of range\n");
		return -EINVAL;
	}
	assert(tw);
	/*
	 * Make sure the expiration time is rounded
	 * up past the current bucket.
	 */
	expires = percpu_get(now_us) + usecs + MIN_DELAY_US - 1;
	//printf("__timer_add now %lu | %lu - %lu = %lu \n",percpu_get(now_us),expires,t->expires,expires-t->expires);
	t->expires = expires;
	timer_insert(tw,t, usecs, expires);

	return 0;
}

int timer_add(struct timer *t, uint64_t usecs)
{
	struct timerwheel *tw = &perfg_get(timer_wheel_fg);
	return __timer_add(tw,t,usecs);
}

int timer_percpu_add(struct timer *t, uint64_t usecs)
{
	struct timerwheel *tw = &percpu_get(timer_wheel_cpu);
	assert(usecs>0);
	return __timer_add(tw,t,usecs);
}


/**
 * timer_add_for_next_tick - adds a timer with the shortest possible delay
 * @t: the timer
 *
 * The timer is added to the nearest bucket and will fire the next
 * time timer_run() is called, assuming MIN_DELAY_US has elapsed.
 */
void timer_add_for_next_tick(struct timer *t)
{
	struct timerwheel *tw = &perfg_get(timer_wheel_fg);
	uint64_t expires;
	expires = percpu_get(now_us) + MIN_DELAY_US;
	t->expires = expires;
	timer_insert(tw,t, MIN_DELAY_US, expires);
}

static void timer_run_bucket(struct timerwheel *tw, struct hlist_head *h)
{
	struct hlist_node *n, *tmp;
	struct timer *t;
#ifdef ENABLE_KSTATS
	kstats_accumulate save;
#endif

	hlist_for_each_safe(h, n, tmp) {
		t = hlist_entry(n, struct timer, link);
		__timer_del(t);
		KSTATS_PUSH(timer_handler, &save);
		t->handler(t);
		KSTATS_POP(&save);
	}
	h->head = NULL;
}

static void timer_reinsert_bucket(struct timerwheel *tw, struct hlist_head *h, uint64_t now_us)
{
	struct hlist_node *x, *tmp;
	struct timer *t;
#ifdef ENABLE_KSTATS
	kstats_accumulate save;
#endif

	hlist_for_each_safe(h, x, tmp) {
		t = hlist_entry(x, struct timer, link);
		__timer_del(t);

		if (timer_expired(tw, t)) {
			KSTATS_PUSH(timer_handler, &save);
			t->handler(t);
			KSTATS_POP(&save);
			continue;
		}
 		timer_insert(tw, t, t->expires - now_us, t->expires);
	}
}

/**
 * timer_collapse - collapselonger-term buckets into shorter-term buckets 
*/

static void timer_collapse(struct timerwheel *tw, uint64_t pos)
{
	int med_off = WHEEL_OFFSET(pos, 1);
	timer_reinsert_bucket(tw, &tw->wheels[1][med_off],pos);
	
	if (!med_off) {
		int low_off = WHEEL_OFFSET(pos, 2);
		timer_reinsert_bucket(tw, &tw->wheels[2][low_off],pos);
		if (tw == &percpu_get(timer_wheel_cpu)) {
			//printf("timer_collapse(low)  off %d %lu\n",low_off, pos);
		}
	}

}

/**
 * timer_run - the main timer processing pass
 * 
 * Call this once per device polling pass.
 */

void timer_run(void)
{

	uint64_t pos = percpu_get(timer_pos);
	struct wheel0fg_hint *hint = &percpu_get(wheel0fg_hint);
	percpu_get(now_us) = rdtsc() / cycles_per_us;

	for (; pos <= percpu_get(now_us); pos += MIN_DELAY_US) {
		int i;
		int high_off = WHEEL_OFFSET(pos, 0);

		if (!high_off) {
			/* warning -- O(#wheel entries * # flow groups) */
			timer_collapse(&percpu_get(timer_wheel_cpu),pos);
			for (i = 0; i < nr_flow_groups; i++) {
				if (fgs[i]->cur_cpu != percpu_get(cpu_id))
					continue;
				eth_fg_set_current(fgs[i]);
				timer_collapse(fg_timerwheel[i],pos);
			}
		}
		if (hint->hints[high_off]) {
			struct timerwheel *tw = &percpu_get(timer_wheel_cpu);
			timer_run_bucket(tw,&tw->wheels[0][high_off]);
			for (i = 0; i < nr_flow_groups; i++) {
				tw = fg_timerwheel[i];
				if (fgs[i]->cur_cpu != percpu_get(cpu_id))
					continue;
				if (hint->hints[high_off] & tw->wheelhint_mask) {
					eth_fg_set_current(fgs[i]);
					timer_run_bucket(tw,&tw->wheels[0][high_off]);
				} else {
#ifdef DEBUG_TIMER
					assert(hlist_empty(&tw->wheels[0][high_off]));
#endif
				}
			}
			hint->hints[high_off] = 0;
		} else {
#ifdef DEBUG_TIMER
			// debug only statement;
			// FIXME: on rebalance, must set all hints for the FG
			assert (hlist_empty(&percpu_get(timer_wheel_cpu).wheels[0][high_off]));
			for (i = 0; i < nr_flow_groups; i++) {
                                if (fgs[i]->cur_cpu != percpu_get(cpu_id))
                                        continue;
                                assert(hlist_empty(&fg_timerwheel[i]->wheels[0][high_off]));
			}
#endif
		}
	}
	percpu_get(timer_pos) = pos;
}


/**
 * timer_deadline - determine the immediate timer deadline
 * 
 * because of its implementation, will return [0..4ms], which is the high wheel resolution
 */
uint64_t
timer_deadline(uint64_t max_deadline_us)
{
	uint64_t pos = percpu_get(timer_pos);
        uint64_t now_us = rdtsc() / cycles_per_us;
        uint64_t max_us = now_us + max_deadline_us;
	
        for (; pos <= max_us; pos += MIN_DELAY_US) {
		int high_off = WHEEL_OFFSET(pos, 0);
		if (!high_off)
			break;  // don't attempt to collapse; just stop at that side of the wheel
		if (percpu_get(wheel0fg_hint).hints[high_off])
			break; // pending event (at least likely; could be more granular)
        }
	
        if (pos < now_us)
		return 0;
        return pos - now_us;
}

/**
 * timer_addfg 
 * @fgid: new flow group now owned by the current core
 */

void
timer_addfg(int fgid)
{
	int i;
	struct timerwheel *tw = fg_timerwheel[fgid];
	assert(tw);
	for (i=0;i<WHEEL_SIZE;i++) { 
		percpu_get(wheel0fg_hint).hints[i] |= tw->wheelhint_mask;
	}
}


/* derived from DPDK */
static int
timer_calibrate_tsc(void)
{
	struct timespec sleeptime = {.tv_nsec = 5E8 }; /* 1/2 second */
	struct timespec t_start, t_end;

	cpu_serialize();
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &t_start) == 0) {
		uint64_t ns, end, start;
		double secs;

		start = rdtsc();
		nanosleep(&sleeptime,NULL);
		clock_gettime(CLOCK_MONOTONIC_RAW, &t_end);
		end = rdtscp(NULL);
		ns = ((t_end.tv_sec - t_start.tv_sec) * 1E9);
		ns += (t_end.tv_nsec - t_start.tv_nsec);

		secs = (double)ns / 1000;
		cycles_per_us = (uint64_t)((end - start)/secs);
		log_info("timer: detected %d ticks per US\n",
			 cycles_per_us);
                return 0;
        }

	return -1;
}

/**
 * timer_init_fg - initializes the timer service for a core
 */
void timer_init_fg(void)
{
	struct timerwheel *tw = &perfg_get(timer_wheel_fg);
	int id = perfg_get(fg_id);
	assert (fg_timerwheel[id] == NULL);
	fg_timerwheel[id] = tw;
	int hint = id % 64;
	tw->wheelhint_mask = (1LL << hint);
	//printf("initializing fg wheel %d mask 0x%lx\n", id, tw->wheelhint_mask);
}

void timer_init_cpu(void)
{
	percpu_get(now_us) = rdtsc() / cycles_per_us;
	percpu_get(timer_pos) = percpu_get(now_us);
	percpu_get(timer_wheel_cpu).wheelhint_mask = 1; 
}	
/**
 * timer_init - global timer initialization
 *
 * Returns 0 if successful, otherwise fail.
 */
int timer_init(void)
{
	int ret;

	ret = timer_calibrate_tsc();
	if (ret)
		return ret;

	return 0;
}

