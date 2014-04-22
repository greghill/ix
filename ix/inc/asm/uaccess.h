/*
 * uaccess.h - routines for safely accessing user memory
 */

#pragma once

#include <ix/errno.h>

#define ASM_REGISTER_FIXUP(fault_addr, fix_addr) \
	".pushsection \"__fixup_tbl\",\"a\"\n"	 \
	".balign 16\n"				 \
	".quad(" #fault_addr ")\n"		 \
	".quad(" #fix_addr ")\n"		 \
	".popsection\n"

#define ASM_START_FIXUP	".section .fixup,\"ax\"\n"
#define ASM_END_FIXUP	".previous\n"

extern volatile int uaccess_fault;

/**
 * uaccess_peekq - safely read a 64-bit word of memory
 * @addr: the address
 *
 * Returns the value.
 */
static inline uint64_t uaccess_peekq(const uint64_t *addr)
{
	uint64_t ret;

	asm volatile("1: movq (%2), %0\n"
		     "2:\n"
		     ASM_START_FIXUP
		     "3: movl $1, %1\n"
		     "jmp 2b\n"
		     ASM_END_FIXUP
		     ASM_REGISTER_FIXUP(1b, 3b) :
		     "=r" (ret), "=m" (uaccess_fault) :
		     "g" (addr) : "memory", "cc");

	return ret;
}

/**
 * uaccess_pokeq - safely writes a 64-bit word of memory
 * @addr: the address
 * @val: the value to write
 */
static inline void uaccess_pokeq(uint64_t *addr, uint64_t val)
{
	asm volatile("1: movq %1, (%2)\n"
		     "2:\n"
		     ASM_START_FIXUP
		     "3: movl $1, %0\n"
		     "jmp 2b\n"
		     ASM_END_FIXUP
		     ASM_REGISTER_FIXUP(1b, 3b) :
		     "=m" (uaccess_fault) :
		     "r" (val), "g" (addr) : "memory", "cc");
}

/**
 * uaccess_check_fault - determines if a peek or poke caused a fault
 *
 * Returns true if there was a fault, otherwise false.
 */
static inline bool uaccess_check_fault(void)
{
	if (uaccess_fault) {
		uaccess_fault = 0;
		return true;
	}

	return false;
}

/**
 * uaccess_copy_user - copies memory from or to the user safely
 * @src: the source address
 * @dst: the destination address
 * @len: the number of bytes to copy
 *
 * Returns 0 if successful, otherwise -EFAULT if there was a bad address.
 */
static inline int uaccess_copy_user(const char *src, char *dst, int len)
{
	int ret;

	asm volatile("1: rep\n"
		     "movsb\n"
		     "xorl %0, %0\n"
		     "2:\n"
		     ASM_START_FIXUP
		     "3:movl %c[errno], %0\n"
		     "jmp 2b\n"
		     ASM_END_FIXUP
		     ASM_REGISTER_FIXUP(1b, 3b) :
		     "=r" (ret), "+S" (src), "+D" (dst), "+c" (len) :
		     [errno]"i"(-EFAULT) : "memory");

	return ret;
}

