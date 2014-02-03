/*
 * mempool.h - a fast fixed-sized memory pool allocator
 */

#pragma once

#include <ix/stddef.h>
#include <ix/mem.h>

struct mempool_hdr {
	struct mempool_hdr *next;
} __packed;

struct mempool {
	void *buf;
	struct mempool_hdr *head;
	physaddr_t *phys_addrs;
	int nr_pages;
};

/**
 * mempool_alloc - allocates an element from a memory pool
 * @m: the memory pool
 *
 * Returns a pointer to the allocated element or NULL if unsuccessful.
 */
static inline void *mempool_alloc(struct mempool *m)
{
	struct mempool_hdr *h = m->head;

	if (likely(h))
		m->head = h->next;

	return (void *) h;
}

/**
 * mempool_free - frees an element back in to a memory pool
 * @m: the memory pool
 * @ptr: the element
 *
 * NOTE: Must be the same memory pool that it was allocated from
 */
static inline void mempool_free(struct mempool *m, void *ptr)
{
	struct mempool_hdr *elem = (struct mempool_hdr *) ptr;

	elem->next = m->head;
	m->head = elem;
}

/**
 * mempool_get_phys - gets the physical address of an element
 * @m: the memory pool
 * @ptr: the element
 *
 * NOTE: This routine can only be used if the mempool was created
 * with mempool_create_phys(). Otherwise there will be unpredicable
 * results.
 *
 * Returns a physical address.
 */
static inline physaddr_t mempool_get_phys(struct mempool *m, void *ptr)
{
	return m->phys_addrs[PGN_2MB(ptr)] + PGOFF_2MB(ptr);
}

extern int mempool_create(struct mempool *m, int nr_elems, size_t elem_len);
extern int mempool_create_phys(struct mempool *m, int nr_elems,
			       size_t elem_len);
extern void mempool_destroy(struct mempool *m);

