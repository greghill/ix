/*
 * vm.h - virtual memory management support
 *
 * NOTE: right now we mostly use Dune for these operations, so this file
 * serves only to augment those capabilities.
 */

#pragma once

#include <ix/lock.h>

DECLARE_SPINLOCK(vm_lock);

#define VM_PERM_R	0x1
#define VM_PERM_W	0x2
#define VM_PERM_X	0x4
#define VM_PERM_U	0x8

extern int __vm_map_phys(physaddr_t pa, virtaddr_t va,
			 int nr, int size, int perm);
extern bool __vm_is_mapped(void *addr, size_t len);

extern int vm_map_phys(physaddr_t pa, virtaddr_t va,
		       int nr, int size, int perm);
extern void *vm_map_to_user(void *kern_addr, int nr, int size, int perm);
extern void vm_unmap(void *addr, int nr, int size);

