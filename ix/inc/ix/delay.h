/*
 * delay.h - tools for pausing the CPU
 */

#pragma once

#include <ix/types.h>

extern void __timer_delay_us(uint64_t us);

/**
 * delay_us - pauses the CPU for microseconds
 * @us: the number of microseconds
 */
static inline void delay_us(uint64_t us)
{
	__timer_delay_us(us);
}

/**
 * delay_ms - pauses the CPU for milliseconds
 * @ms: the number of milliseconds
 */
static inline void delay_ms(uint64_t ms)
{
	__timer_delay_us(ms * 1000);
}

