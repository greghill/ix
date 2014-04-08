/*
 * syscall.h - system call support
 */

#pragma once

#include <ix/syscall.h>
#include <ix/mem.h>
#include <ix/vm.h>
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

static inline int sys_mmap(void *addr, int nr, int size, int perm)
{
	return (int) SYSCALL(SYS_MMAP, addr, nr, size, perm);
}

static inline int sys_unmap(void *addr, int nr, int size)
{
	return (int) SYSCALL(SYS_MUNMAP, addr, nr, size);
}

static inline int sys_spawnmode(bool spawn_cores)
{
	return (int) SYSCALL(SYS_SPAWNMODE, spawn_cores);
}

static inline int sys_nrcpus(void)
{
	return (int) SYSCALL(SYS_NRCPUS);
}

