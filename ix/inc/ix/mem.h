/*
 * mem.h - memory management
 */

#pragma once

#include <numaif.h>
#include <numa.h>

enum {
	PGSHIFT_4KB = 12,
	PGSHIFT_2MB = 21,
	PGSHIFT_1GB = 30,
};

enum {
	PGSIZE_4KB = (1 << PGSHIFT_4KB), /* 4096 bytes */
	PGSIZE_2MB = (1 << PGSHIFT_2MB), /* 2097152 bytes */
	PGSIZE_1GB = (1 << PGSHIFT_1GB), /* 1073741824 bytes */
};

#define PGMASK_4KB	(PGSIZE_4KB - 1)
#define PGMASK_2MB	(PGSIZE_2MB - 1)
#define PGMASK_1GB	(PGSIZE_1GB - 1)

/* page numbers */
#define PGN_4KB(la)	(((uintptr_t) (la)) >> PGSHIFT_4KB)
#define PGN_2MB(la)	(((uintptr_t) (la)) >> PGSHIFT_2MB)
#define PGN_1GB(la)	(((uintptr_t) (la)) >> PGSHIFT_1GB)

#define PGOFF_4KB(la)	(((uintptr_t) (la)) & PGMASK_4KB)
#define PGOFF_2MB(la)	(((uintptr_t) (la)) & PGMASK_2MB)
#define PGOFF_1GB(la)	(((uintptr_t) (la)) & PGMASK_1GB)

#define PGADDR_4KB(la)	(((uintptr_t) (la)) & ~((uintptr_t) PGMASK_4KB))
#define PGADDR_2MB(la)	(((uintptr_t) (la)) & ~((uintptr_t) PGMASK_2MB))
#define PGADDR_1GB(la)	(((uintptr_t) (la)) & ~((uintptr_t) PGMASK_1GB))

/*
 * numa policy values: (defined in numaif.h)
 * MPOL_DEFAULT - use the process' global policy
 * MPOL_BIND - force the numa mask
 * MPOL_PREFERRED - use the numa mask but fallback on other memory
 * MPOL_INTERLEAVE - interleave nodes in the mask (good for throughput)
 */

typedef unsigned long physaddr_t;

extern void *
mem_alloc_pages(int nr, int size, struct bitmask *mask, int numa_policy);
extern void *
mem_alloc_pages_onnode(int nr, int size, int node, int numa_policy);
extern void mem_free_pages(void *addr, int nr, int size);
extern int mem_lookup_page_phys_addrs(void *addr, int nr, int size,
				      physaddr_t *paddrs);

/**
 * mem_alloc_page - allocates a page of memory
 * @size: the page size (4KB, 2MB, or 1GB)
 * @numa_node: the numa node to allocate the page from
 * @numa_policy: how strictly to take @numa_node
 *
 * Returns a pointer (virtual address) to a page or NULL if fail.
 */
static inline void *mem_alloc_page(int size, int numa_node, int numa_policy)
{
	return mem_alloc_pages_onnode(1, size, numa_node, numa_policy);
}

/**
 * mem_alloc_page_local - allocates a page of memory on the local numa node
 * @size: the page size (4KB, 2MB, or 1GB)
 *
 * NOTE: This variant is best effort only. If no pages are available on the
 * local node, a page from another NUMA node could be used instead. If you
 * need stronger assurances, use mem_alloc_page().
 *
 * Returns a pointer (virtual address) to a page or NULL if fail.
 */
static inline void *mem_alloc_page_local(int size)
{
	return mem_alloc_pages(1, size, NULL, MPOL_PREFERRED);
}

/**
 * mem_free_page - frees a page of memory
 * @addr: a pointer to the page
 * @size: the page size (4KB, 2MB, or 1GB)
 */
static inline void mem_free_page(void *addr, int size)
{
	return mem_free_pages(addr, 1, size);
}

/**
 * mem_lookup_page_phys_addr - determines the host-physical address of a page
 * @addr: a pointer to the page
 * @size: the page size (4KB, 2MB, or 1GB)
 * @paddr: a pointer to store the result
 *
 * Returns 0 if successful, otherwise failure.
 */
static inline int
mem_lookup_page_phys_addr(void *addr, int size, physaddr_t *paddr)
{
	return mem_lookup_page_phys_addrs(addr, 1, size, paddr);
}

