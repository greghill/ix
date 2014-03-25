/*
 * vm.h - virtual memory management support
 *
 * NOTE: right now we mostly use Dune for these operations, so this file
 * serves only to augment those capabilities.
 */

#pragma once

#include <ix/mem.h>
#include <ix/lock.h>

/* FIXME: this should be defined in inc/asm */
#include <mmu-x86.h>

DECLARE_SPINLOCK(vm_lock);

/* FIXME: a bunch of gross hacks until we can better integrate libdune */
#define UINT64(x) ((uint64_t) x)
#define CAST64(x) ((uint64_t) x)
typedef uint64_t ptent_t;
#define NPTBITS	9
#ifndef pgroot
extern ptent_t *pgroot;
#endif

/**
 * vm_lookup_phys - determine a physical address from a virtual address
 * @virt: the virtual address
 * @pgsize: the size of the page at the address (must be correct).
 *
 * Returns a physical address.
 */
static inline physaddr_t vm_lookup_phys(void *virt, int pgsize)
{
	ptent_t *dir = pgroot;
	ptent_t pte;

	pte = dir[PDX(3, virt)];
	if (!(PTE_FLAGS(pte) & PTE_P))
		return 0;

	dir = (ptent_t *) PTE_ADDR(pte);
	pte = dir[PDX(2, virt)];
	if (!(PTE_FLAGS(pte) & PTE_P))
		return 0;
	if (pgsize == PGSIZE_1GB)
		return (physaddr_t) PTE_ADDR(pte);

	dir = (ptent_t *) PTE_ADDR(pte);
	pte = dir[PDX(1, virt)];
	if (!(PTE_FLAGS(pte) & PTE_P))
		return 0;
	if (pgsize == PGSIZE_2MB)
		return (physaddr_t) PTE_ADDR(pte);

	dir = (ptent_t *) PTE_ADDR(pte);
	pte = dir[PDX(0, virt)];
	if (!(PTE_FLAGS(pte) & PTE_P))
		return 0;
	return (physaddr_t) PTE_ADDR(pte);
}

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

