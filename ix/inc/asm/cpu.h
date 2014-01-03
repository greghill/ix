/*
 * cpu.h - definitions for x86_64 CPUs
 */

#pragma once

/*
 * Endianness
 */

#define __LITTLE_ENDIAN	1234
#define __BIG_ENDIAN	4321

#define __BYTE_ORDER	__LITTLE_ENDIAN


/*
 * Word Size
 */

#define __32BIT_WORDS	32
#define __64BIT_WORDS	64

#define __WORD_SIZE	__64BIT_WORDS

#define cpu_relax() \
	asm volatile("pause")

static inline unsigned long rdtscll(void)
{
	unsigned int a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((unsigned long) a) | (((unsigned long) d) << 32);
}

