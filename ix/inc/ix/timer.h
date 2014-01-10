/*
 * timer.h - timer event infrastructure
 */

#pragma once

#include <ix/list.h>

struct timer {
	struct hlist_node link;
	void (*handler)(struct timer *t);
	uint64_t expires;
};

#define ONE_SECOND	1000000
#define ONE_MS		10000
#define ONE_US		1
/**
 * timer_init_entry - initializes a timer
 * @t: the timer
 */
static inline void
timer_init_entry(struct timer *t, void (*handler)(struct timer *t))
{
	t->link.prev = NULL;
	t->handler = handler;
}

/**
 * timer_pending - determines if a timer is pending
 * @t: the timer
 *
 * Returns true if the timer is pending, otherwise false.
 */
static inline bool timer_pending(struct timer *t)
{
	return t->link.prev != NULL;
}

extern int timer_add(struct timer *t, uint64_t usecs);
extern void timer_add_for_next_tick(struct timer *t);

static inline void __timer_del(struct timer *t)
{
	hlist_del(&t->link);
	t->link.prev = NULL;
}

/**
 * timer_mod - modifies a timer
 * @t: the timer
 * @usecs: the number of microseconds from present to fire the timer
 *
 * If the timer is already armed, then its trigger time is modified.
 * Otherwise this function behaves like timer_add().
 *
 * Returns 0 if successful, otherwise failure.
 */
static inline int timer_mod(struct timer *t, uint64_t usecs)
{
	if (timer_pending(t))
		__timer_del(t);
	return timer_add(t, usecs);
}

/**
 * timer_del - disarms a timer
 * @t: the timer
 *
 * If the timer is already disarmed, then nothing happens.
 */
static inline void timer_del(struct timer *t)
{
	if (timer_pending(t))
		__timer_del(t);
}

extern void timer_run(void);
extern int timer_init(void);

