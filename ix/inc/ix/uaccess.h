/*
 * uaccess.h - routines for safely accessing user memory
 */

#pragma once

#include <ix/types.h>
#include <ix/mem.h>
#include <ix/errno.h>

#include <asm/uaccess.h>

/**
 * uaccess_okay - determines if a memory object lies in userspace
 * @addr: the address
 * @len: the length of the memory object
 *
 * Returns true if the memory is user-level, otherwise false.
 */
static inline bool uaccess_okay(void *addr, size_t len)
{
	if (len > MEM_USER_END - MEM_USER_START ||
	    (uintptr_t) addr < MEM_USER_START ||
	    (uintptr_t) addr + len > MEM_USER_END)
		return false;
	return true;
}

/**
 * uaccess_zc_okay - determines if a memory object is safe to zero-copy
 * @addr: the address
 * @len: the length of the memory object
 *
 * Returns true if the memory is safe for zero-copy, otherwise false.
 */
static inline bool uaccess_zc_okay(void *addr, size_t len)
{
	if (len > MEM_ZC_USER_END - MEM_ZC_USER_START ||
	    (uintptr_t) addr < MEM_ZC_USER_START ||
	    (uintptr_t) addr + len > MEM_ZC_USER_END)
		return false;
	return true;
}

/**
 * copy_from_user - safely copies user memory to kernel memory
 * @user_src: the user source memory
 * @kern_dst: the kernel destination memory
 * @len: the number of bytes to copy
 *
 * Returns 0 if successful, otherwise -EFAULT if unsafe.
 */
static inline int copy_from_user(void *user_src, void *kern_dst, size_t len)
{
	if (!uaccess_okay(user_src, len))
		return -EFAULT;

	return uaccess_copy_user(user_src, kern_dst, len);
}

/**
 * copy_to_user - safely copies kernel memory to user memory
 * @kern_src: the kernel source memory
 * @user_dst: the user destination memory
 * @len: the number of bytes to copy
 *
 * Returns 0 if successful, otherwise -EFAULT if unsafe.
 */
static inline int copy_to_user(void *kern_src, void *user_dst, size_t len)
{
	if (!uaccess_okay(user_dst, len))
		return -EFAULT;

	return uaccess_copy_user(kern_src, user_dst, len);
}

