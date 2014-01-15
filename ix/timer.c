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

static struct hlist_head wheels[WHEEL_COUNT][WHEEL_SIZE];
static uint64_t now_us, timer_pos;

static inline bool timer_expired(struct timer *t)
{
	return (t->expires <= now_us);
}

static inline struct hlist_head *
__timer_get_bucket(uint64_t left, uint64_t expires)
{
	int index = ((63 - clz64(left | MIN_DELAY_US) - MIN_DELAY_SHIFT)
			>> WHEEL_SHIFT_LOG2);
	int offset = WHEEL_OFFSET(expires, index);

	return &wheels[index][offset];
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
	expires = now_us + usecs + MIN_DELAY_US - 1;
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
	expires = now_us + MIN_DELAY_US;
	t->expires = expires;
	timer_insert(t, MIN_DELAY_US, expires);
}

static void timer_run_bucket(struct hlist_head *h)
{
	struct hlist_node *n;
	struct timer *t;

	hlist_for_each(h, n) {
		t = hlist_entry(n, struct timer, link);
		n->prev = NULL;
		t->handler(t);
	}
	h->head = NULL;
}

static void timer_reinsert_bucket(struct hlist_head *h)
{
	struct hlist_node *pos, *tmp;
	struct timer *t;

	hlist_for_each_safe(h, pos, tmp) {
		t = hlist_entry(pos, struct timer, link);
		__timer_del(t);

		if (timer_expired(t)) {
			t->handler(t);
			continue;
		}

		timer_insert(t, t->expires - now_us, t->expires);
	}
}

/* FIXME: we need the control plane to provide this information */
#define CPU_CLOCKS_PER_US	3400

/**
 * timer_run - the main timer processing pass
 * 
 * Call this once per device polling pass.
 */
void timer_run(void)
{
	now_us = rdtscll() / CPU_CLOCKS_PER_US;

	for (; timer_pos <= now_us; timer_pos += MIN_DELAY_US) {
		int high_off = WHEEL_OFFSET(timer_pos, 0);

		/* collapse longer-term buckets into shorter-term buckets */
		if (!high_off) {
			int med_off = WHEEL_OFFSET(timer_pos, 1);
			timer_reinsert_bucket(&wheels[1][med_off]);

			if (!med_off) {
				int low_off = WHEEL_OFFSET(timer_pos, 2);
				timer_reinsert_bucket(&wheels[2][low_off]);
			}
		}

		timer_run_bucket(&wheels[0][high_off]);
	}
}

/**
 * timer_init - initializes the timer service
 *
 * Returns 0 if successful, otherwise fail.
 */
int timer_init(void)
{
	now_us = rdtscll() / CPU_CLOCKS_PER_US;
	timer_pos = now_us;
	return 0;
}

