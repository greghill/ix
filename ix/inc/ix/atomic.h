/*
 * atomic.h - utilities for atomically manipulating memory
 */

#pragma once

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

