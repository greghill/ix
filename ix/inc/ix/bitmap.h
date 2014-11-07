/*
 * bitmap.h - a library for bit array manipulation
 */

#pragma once

#include <ix/stddef.h>

#define BITS_PER_LONG	(sizeof(long) * 8)
#define BITMAP_LONG_SIZE(nbits) \
	div_up(nbits, BITS_PER_LONG)

#define DEFINE_BITMAP(name, nbits) \
	unsigned long name[BITMAP_LONG_SIZE(nbits)]

typedef unsigned long *bitmap_ptr;

#define BITMAP_POS_IDX(pos)	((pos) / BITS_PER_LONG)
#define BITMAP_POS_SHIFT(pos)	((pos) & (BITS_PER_LONG - 1))

/**
 * bitmap_set - sets a bit in the bitmap
 * @bits: the bitmap
 * @pos: the bit number
 */
static inline void bitmap_set(unsigned long *bits, int pos)
{
	bits[BITMAP_POS_IDX(pos)] |= (1 << BITMAP_POS_SHIFT(pos));
}

/**
 * bitmap_clear - clears a bit in the bitmap
 * @bits: the bitmap
 * @pos: the bit number
 */
static inline void bitmap_clear(unsigned long *bits, int pos)
{
	bits[BITMAP_POS_IDX(pos)] &= ~(1 << BITMAP_POS_SHIFT(pos));
}

/**
 * bitmap_test - tests if a bit is set in the bitmap
 * @bits: the bitmap
 * @pos: the bit number
 *
 * Returns true if the bit is set, otherwise false.
 */
static inline bool bitmap_test(unsigned long *bits, int pos)
{
	return (bits[BITMAP_POS_IDX(pos)] & (1 << BITMAP_POS_SHIFT(pos))) != 0;
}

/**
 * bitmap_init - initializes a bitmap
 * @bits: the bitmap
 * @nbits: the number of total bits
 * @state: if true, all bits are set, otherwise all bits are cleared
 */
static inline void bitmap_init(unsigned long *bits, int nbits, bool state)
{
	memset(bits, state ? 0xff : 0x00, BITMAP_LONG_SIZE(nbits) * sizeof(long));
}

