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

#define cpu_serialize() \
	asm volatile("cpuid" : : : "%rax", "%rbx", "%rcx", "%rdx");

static inline unsigned long rdtsc(void)
{
	unsigned int a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return ((unsigned long) a) | (((unsigned long) d) << 32);
}

static inline unsigned long rdtscp(unsigned int *aux)
{
	unsigned int a, d, c;
	asm volatile("rdtscp" : "=a" (a), "=d" (d), "=c" (c));
	if (aux)
		*aux = c;
	return ((unsigned long) a) | (((unsigned long) d) << 32);
}

