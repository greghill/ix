/*
 * timer.c - timer event infrastructure
 *
 * Assumption #1: most timers will be canceled before they fire.
 * Assumption #2: precision matters less the longer the delay.
 * Assumption #3: division instructions are very expensive.
 *
 * The design is inspired by "Hashed and Hierarchical Timing Wheels: Data
 * Structures for the Efficient Implementation of a Timer Facility" by
 * George Varghese and Tony Lauck. SOSP 87.
 *
 * Specificially, we use Scheme 7 described in the paper, where
 * hierarchical sets of buckets are used. We use the variant where
 * timers are unsorted in each bucket in order to trade some accuracy for
 * greater efficiency.
 */

#include <ix/timer.h>
#include <ix/errno.h>

#define WHEEL_SHIFT_LOG2	3
#define WHEEL_SHIFT		(1 << WHEEL_SHIFT_LOG2)
#define WHEEL_SIZE		(1 << WHEEL_SHIFT)
#define WHEEL_MASK		(WHEEL_SIZE - 1)
#define WHEEL_COUNT		3

#define MIN_DELAY_SHIFT		4
#define MIN_DELAY_US		(1 << MIN_DELAY_SHIFT)
#define MIN_DELAY_MASK		(MIN_DELAY_US - 1)
#define MAX_DELAY_US		(MIN_DELAY_US * (1 << (WHEEL_COUNT * WHEEL_SHIFT)))

/*
 * NOTE: these paramemters may need to be tweaked.
 *
 * Right now we have the following wheels:
 *
 * high precision wheel: 256 x 16 us increments
 * medium precision wheel: 256 x 4 ms increments
 * low precision wheel: 256 x 1 second increments
 *
 * Total range 0 to 256 seconds...
 */

static struct hlist_head wheels[WHEEL_COUNT][WHEEL_SIZE];
static uint64_t last_us, now_us;

static inline struct hlist_head *__timer_get_bucket(uint64_t usecs)
{
	uint64_t expires = now_us + usecs;
	int index = ((64 - clz64(usecs | MIN_DELAY_MASK) - MIN_DELAY_SHIFT)
			>> WHEEL_SHIFT_LOG2);
	int shift = (index << WHEEL_SHIFT_LOG2);
	int offset = ((((expires - 1) >> shift) + 1) & WHEEL_MASK);

	return &wheels[index][offset];
}

int __timer_start(struct timer_list *l, uint64_t usecs)
{
	struct hlist_head *bucket;
	if (unlikely(usecs >= MAX_DELAY_US))
		return -EINVAL;

	bucket = __timer_get_bucket(usecs);
	hlist_add_head(bucket, &l->link);
	return 0;
}

/* FIXME: we need the control plane to provide this information */
#define CPU_CLOCKS_PER_US	3400

/**
 * timer_update - prepares for timers to be added and for timer processing
 * 
 * This must be called recently, typically at the very beginning of a
 * processing pass. The following sequence is recommended:
 * 
 * --- Enter Kernel ---
 * 1.) timer_update()
 * 2.) add a bunch of timers
 * 3.) timer_run()
 * --- Exit Kernel ---
 */
void timer_update(void)
{
	/* 
	 * NOTE: there are purportedly some pretty large performance
	 * benefits to performing 32-bit division instead of 64-bit
	 * division. Sadly, our low precision wheel causes us to
	 * require more than 32-bits of the TSC.
	 * 
	 * FIXME: this should be made portable.
	 */
	uint32_t tsc_high, tsc_low;
	uint64_t tsc;

	last_us = now_us;
	asm volatile("rdtsc" : "=a" (tsc_low), "=d" (tsc_high));
	tsc = ((uint64_t) tsc_low) | (((uint64_t) tsc_high) << 32);
	now_us = tsc / CPU_CLOCKS_PER_US;
}

static void timer_run_one_wheel(int idx, uint64_t start, uint64_t stop)
{
	uint64_t i;
	struct hlist_head *h;
	struct hlist_node *n;
	struct timer_list *l;

	for (i = start + 1; i <= stop; i++) {
		h = &wheels[idx][i & WHEEL_MASK];
		hlist_for_each(h, n) {
			l = hlist_entry(n, struct timer_list, link);
			l->handler(l);
			n->prev = NULL;
		}

		h->head = NULL;
	}
}

/**
 * timer_run - the main timer processing pass
 * 
 * Call this once per device polling pass.
 */
void timer_run(void)
{
	uint64_t last = last_us;
	uint64_t now = now_us;

	/* high precision wheel */
	now >>= WHEEL_SHIFT + MIN_DELAY_SHIFT;
	last >>= WHEEL_SHIFT + MIN_DELAY_SHIFT;
	timer_run_one_wheel(0, last, now);

	/* medium precision wheel */
	now >>= WHEEL_SHIFT;
	last >>= WHEEL_SHIFT;
	if (now == last)
		return;
	timer_run_one_wheel(1, last, now);

	/* low precision wheel */
	now >>= WHEEL_SHIFT;
	last >>= WHEEL_SHIFT;
	if (now == last)
		return;
	timer_run_one_wheel(2, last, now);
}
