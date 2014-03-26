/*
 * syscall.h - system call support
 */

#pragma once

#include <ix/syscall.h>
#include "syscall_raw.h"

static inline int sys_bpoll(struct bsys_desc *d, unsigned int nr)
{
	return (int) SYSCALL(SYS_BPOLL, d, nr);
}

static inline int sys_bcall(struct bsys_desc *d, unsigned int nr)
{
	return (int) SYSCALL(SYS_BCALL, d, nr);
}

static inline void *sys_baddr(void)
{
	return (struct bsys_arr *) SYSCALL(SYS_BADDR);
}


