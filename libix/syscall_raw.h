/*
 * syscall_raw.h - low-level system call support
 */

#pragma once

static inline long __syscall_0(long sysnr)
{
	register long ret asm("rax");

	asm volatile("syscall" : "=a" (ret) :
		     "0" (sysnr + SYSCALL_START) :
		     "memory", "cx", "r11");

	return ret;
}

static inline long __syscall_1(long sysnr, long arg0)
{
	register long ret asm("rax");
	register long rdi asm("rdi") = arg0;

	asm volatile("syscall" : "=a" (ret) :
		     "0" (sysnr + SYSCALL_START), "r" (rdi) :
		     "memory", "cx", "r11");

	return ret;
}

static inline long __syscall_2(long sysnr, long arg0, long arg1)
{
	register long ret asm("rax");
	register long rdi asm("rdi") = arg0;
	register long rsi asm("rsi") = arg1;

	asm volatile("syscall" : "=a" (ret) :
		     "0" (sysnr + SYSCALL_START), "r" (rdi), "r" (rsi) :
		     "memory", "cx", "r11");

	return ret;
}

static inline long __syscall_3(long sysnr, long arg0, long arg1, long arg2)
{
	register long ret asm("rax");
	register long rdi asm("rdi") = arg0;
	register long rsi asm("rsi") = arg1;
	register long rdx asm("rdx") = arg2;

	asm volatile("syscall" : "=a" (ret) :
		     "0" (sysnr + SYSCALL_START), "r" (rdi), "r" (rsi),
		     "r" (rdx) :
		     "memory", "cx", "r11");

	return ret;
}

static inline long __syscall_4(long sysnr, long arg0, long arg1, long arg2,
			       long arg3)
{
	register long ret asm("rax");
	register long rdi asm("rdi") = arg0;
	register long rsi asm("rsi") = arg1;
	register long rdx asm("rdx") = arg2;
	register long r10 asm("r10") = arg3;

	asm volatile("syscall" : "=a" (ret) :
		     "0" (sysnr + SYSCALL_START), "r" (rdi), "r" (rsi),
		     "r" (rdx), "r" (r10) :
		     "memory", "cx", "r11");

	return ret;
}

static inline long __syscall_5(long sysnr, long arg0, long arg1, long arg2,
			       long arg3, long arg4)
{
	register long ret asm("rax");
	register long rdi asm("rdi") = arg0;
	register long rsi asm("rsi") = arg1;
	register long rdx asm("rdx") = arg2;
	register long r10 asm("r10") = arg3;
	register long r8 asm("r8") = arg4;

	asm volatile("syscall" : "=a" (ret) :
		     "0" (sysnr + SYSCALL_START), "r" (rdi), "r" (rsi),
		     "r" (rdx), "r" (r10), "r" (r8) :
		     "memory", "cx", "r11");

	return ret;
}

static inline long __syscall_6(long sysnr, long arg0, long arg1, long arg2,
			       long arg3, long arg4, long arg5)
{
	register long ret asm("rax");
	register long rdi asm("rdi") = arg0;
	register long rsi asm("rsi") = arg1;
	register long rdx asm("rdx") = arg2;
	register long r10 asm("r10") = arg3;
	register long r8 asm("r8") = arg4;
	register long r9 asm("r9") = arg5;

	asm volatile("syscall" : "=a" (ret) :
		     "0" (sysnr + SYSCALL_START), "r" (rdi), "r" (rsi),
		     "r" (rdx), "r" (r10), "r" (r8), "r" (r9) :
		     "memory", "cx", "r11");

	return ret;
}

#define SYSCALL_0(sysnr) \
	__syscall_0(sysnr)
#define SYSCALL_1(sysnr, arga) \
	__syscall_1(sysnr, (long) (arga))
#define SYSCALL_2(sysnr, arga, argb) \
	__syscall_2(sysnr, (long) (arga), (long) (argb))
#define SYSCALL_3(sysnr, arga, argb, argc) \
	__syscall_3(sysnr, (long) (arga), (long) (argb), (long) (argc))
#define SYSCALL_4(sysnr, arga, argb, argc, argd) \
	__syscall_4(sysnr, (long) (arga), (long) (argb), (long) (argc), \
		    (long) (argd))
#define SYSCALL_5(sysnr, arga, argb, argc, argd, arge) \
	__syscall_5(sysnr, (long) (arga), (long) (argb), (long) (argc), \
		    (long) (argd), (long) (arge))
#define SYSCALL_6(sysnr, arga, argb, argc, argd, arge, argf) \
	__syscall_6(sysnr, (long) (arga), (long) (argb), (long) (argc), \
		    (long) (argd), (long) (arge), (long) (argf))

#define __NUM_ARGS(_0, _1, _2, _3, _4, _5, _6, N, ...) N
#define NUM_ARGS(...) __NUM_ARGS(__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)

#define __CONCAT(x, y) x ## y
#define CONCAT(x, y) __CONCAT(x, y)

/**
 * SYSCALL - perform a syscall in IX
 *
 * Returns the result (as a long).
 */
#define SYSCALL(...) \
	CONCAT(SYSCALL_, NUM_ARGS(__VA_ARGS__))(__VA_ARGS__)

