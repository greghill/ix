/*
 * timer.h - timer event infrastructure
 */

#pragma once

#include <ix/list.h>

struct timer_list {
	struct hlist_node link;
	void (*handler)(struct timer_list *l);
};

/**
 * timer_init - initializes a timer
 * @l: the timer
 */
static inline void
timer_init(struct timer_list *l, void (*handler)(struct timer_list *l))
{
	l->link.prev = NULL;
	l->handler = handler;
}

/**
 * timer_pending - determines if a timer is pending
 * @l: the timer
 *
 * Returns true if the timer is pending, otherwise false.
 */
static inline bool timer_pending(struct timer_list *l)
{
	return l->link.prev != NULL;
}

extern int __timer_start(struct timer_list *l, uint64_t usecs);

static inline void __timer_stop(struct timer_list *l)
{
	hlist_del(&l->link);
	l->link.prev = NULL;
}

/**
 * timer_start - arms a timer
 * @l: the timer
 * @usecs: the number of microseconds from the present to fire the timer
 *
 * Note that this can be called more than once, but only the most recent
 * timeout will be applied.
 *
 * Returns 0 if successful, otherwise failure.
 */
static inline int timer_start(struct timer_list *l, uint64_t usecs)
{
	if (timer_pending(l))
		__timer_stop(l);
	return __timer_start(l, usecs);
}

/**
 * timer_stop - disarms a timer
 * @l: the timer
 *
 * If the timer is already disarmed, then nothing is done.
 */
static inline void timer_stop(struct timer_list *l)
{
	if (timer_pending(l))
		__timer_stop(l);
}

extern void timer_update(void);
extern void timer_run(void);
