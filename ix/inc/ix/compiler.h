/*
 * compiler.h - useful compiler hints, intrinsics, and attributes
 */

#pragma once

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define unreachable() __builtin_unreachable()

#define prefetch0(x) __builtin_prefetch((x), 0, 3)
#define prefetch1(x) __builtin_prefetch((x), 0, 2)
#define prefetch2(x) __builtin_prefetch((x), 0, 1)
#define prefetchnta(x) __builtin_prefetch((x), 0, 0)
#define prefetch() prefetch0()

#define clz64(x) __builtin_clzll(x)

#define __packed __attribute__((packed))
#define __notused __attribute__((unused))
#define __aligned(x) __attribute__((aligned (x)))

#define GCC_VERSION (__GNUC__ * 10000        \
		     + __GNUC_MINOR__ * 100  \
		     + __GNUC_PATCHLEVEL__)

#if GCC_VERSION >= 40800
#define HAS_BUILTIN_BSWAP 1
#endif

