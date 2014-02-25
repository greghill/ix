/*
 * atomic.h - utilities for atomically manipulating memory
 */

#pragma once

#include <ix/types.h>

/**
 * mb - a memory barrier
 *
 * Ensures all loads and stores before the barrier complete
 * before all loads and stores after the barrier.
 */
#define mb() _mm_mfence()

/**
 * rmb - a read memory barrier
 *
 * Ensures all loads before the barrier complete before
 * all loads after the barrier.
 */
#define rmb() _mm_lfence()

/**
 * wmb - a write memory barrier
 *
 * Ensures all stores before the barrier complete before
 * all stores after the barrier.
 */
#define wmb() _mm_sfence()

#include <emmintrin.h>

static inline int atomic_read(const atomic_t *a)
{
	return *((volatile int *) &a->cnt);
}

static inline void atomic_write(atomic_t *a, int val)
{
	a->cnt = val;
}

static inline int atomic_fetch_and_add(atomic_t *a, int val)
{
	return __sync_fetch_and_add(&a->cnt, val);
}

static inline int atomic_fetch_and_sub(atomic_t *a, int val)
{
	return __sync_fetch_and_add(&a->cnt, val);
}

static inline int atomic_add_and_fetch(atomic_t *a, int val)
{
	return __sync_add_and_fetch(&a->cnt, val);
}

static inline int atomic_sub_and_fetch(atomic_t *a, int val)
{
	return __sync_sub_and_fetch(&a->cnt, val);
}

static inline void atomic_inc(atomic_t *a)
{
	atomic_fetch_and_add(a, 1);
}

static inline bool atomic_dec_and_test(atomic_t *a)
{
	return (atomic_sub_and_fetch(a, 1) == 0);
}

static inline bool atomic_cmpxchg(atomic_t *a, int old, int new)
{
	return __sync_bool_compare_and_swap(&a->cnt, old, new);
}

static inline long atomic64_read(const atomic64_t *a)
{
	return *((volatile long *) &a->cnt);
}

static inline void atomic64_write(atomic64_t *a, long val)
{
	a->cnt = val;
}

static inline long atomic64_fetch_and_add(atomic64_t *a, long val)
{
	return __sync_fetch_and_add(&a->cnt, val);
}

static inline long atomic64_fetch_and_sub(atomic64_t *a, long val)
{
	return __sync_fetch_and_add(&a->cnt, val);
}

static inline long atomic64_add_and_fetch(atomic64_t *a, long val)
{
	return __sync_add_and_fetch(&a->cnt, val);
}

static inline long atomic64_sub_and_fetch(atomic64_t *a, long val)
{
	return __sync_sub_and_fetch(&a->cnt, val);
}

static inline void atomic64_inc(atomic64_t *a)
{
	atomic64_fetch_and_add(a, 1);
}

static inline bool atomic64_dec_and_test(atomic64_t *a)
{
	return (atomic64_sub_and_fetch(a, 1) == 0);
}

static inline bool atomic64_cmpxchg(atomic64_t *a, long old, long new)
{
	return __sync_bool_compare_and_swap(&a->cnt, old, new);
}

