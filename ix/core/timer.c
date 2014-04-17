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

#include <ix/timer.h>
#include <ix/errno.h>
#include <ix/log.h>
#include <ix/cpu.h>
#include <ix/kstats.h>

#include <time.h>

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

static DEFINE_PERCPU(struct hlist_head, wheels[WHEEL_COUNT][WHEEL_SIZE]);
static DEFINE_PERCPU(uint64_t, now_us);
static DEFINE_PERCPU(uint64_t, timer_pos);
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

static inline bool timer_expired(struct timer *t)
{
	return (t->expires <= percpu_get(now_us));
}

static inline struct hlist_head *
__timer_get_bucket(uint64_t left, uint64_t expires)
{
	int index = ((63 - clz64(left | MIN_DELAY_US) - MIN_DELAY_SHIFT)
			>> WHEEL_SHIFT_LOG2);
	int offset = WHEEL_OFFSET(expires, index);

	return &percpu_get(wheels[index][offset]);
}

static void timer_insert(struct timer *t,
			 uint64_t left, uint64_t expires)
{
	struct hlist_head *bucket;
	bucket = __timer_get_bucket(left, expires);
	hlist_add_head(bucket, &t->link);
}

/**
 * timer_add - adds a timer
 * @l: the timer
 * @usecs: the time interval from present to fire the timer
 *
 * Returns 0 if successful, otherwise failure.
 */
int timer_add(struct timer *t, uint64_t usecs)
{
	uint64_t expires;
	if (unlikely(usecs >= MAX_DELAY_US))
		return -EINVAL;

	/*
	 * Make sure the expiration time is rounded
	 * up past the current bucket.
	 */
	expires = percpu_get(now_us) + usecs + MIN_DELAY_US - 1;
	t->expires = expires;
	timer_insert(t, usecs, expires);

	return 0;
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
	uint64_t expires;
	expires = percpu_get(now_us) + MIN_DELAY_US;
	t->expires = expires;
	timer_insert(t, MIN_DELAY_US, expires);
}

static void timer_run_bucket(struct hlist_head *h)
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

static void timer_reinsert_bucket(struct hlist_head *h)
{
	struct hlist_node *pos, *tmp;
	struct timer *t;
#ifdef ENABLE_KSTATS
	kstats_accumulate save;
#endif

	hlist_for_each_safe(h, pos, tmp) {
		t = hlist_entry(pos, struct timer, link);
		__timer_del(t);

		if (timer_expired(t)) {
			KSTATS_PUSH(timer_handler, &save);
			t->handler(t);
			KSTATS_POP(&save);
			continue;
		}

		timer_insert(t, t->expires - percpu_get(now_us), t->expires);
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
	percpu_get(now_us) = rdtsc() / cycles_per_us;

	for (; pos <= percpu_get(now_us); pos += MIN_DELAY_US) {
		int high_off = WHEEL_OFFSET(pos, 0);

		/* collapse longer-term buckets into shorter-term buckets */
		if (!high_off) {
			int med_off = WHEEL_OFFSET(pos, 1);
			timer_reinsert_bucket(&percpu_get(wheels[1][med_off]));

			if (!med_off) {
				int low_off = WHEEL_OFFSET(pos, 2);
				timer_reinsert_bucket(&percpu_get(wheels[2][low_off]));
			}
		}

		timer_run_bucket(&percpu_get(wheels[0][high_off]));
	}

	percpu_get(timer_pos) = pos;
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
 * timer_init_cpu - initializes the timer service for a core
 */
void timer_init_cpu(void)
{
	percpu_get(now_us) = rdtsc() / cycles_per_us;
	percpu_get(timer_pos) = percpu_get(now_us);
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

