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
	void			*buf;
	struct mempool_hdr	*head;
	int			nr_pages;

#ifdef __KERNEL__
	void 			*iomap_addr;
	uintptr_t		iomap_offset;
#endif
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

extern int mempool_create(struct mempool *m, int nr_elems, size_t elem_len);
extern void mempool_destroy(struct mempool *m);


#ifdef __KERNEL__

/**
 * mempool_pagemem_to_iomap - get the IOMAP address of a mempool entry
 * @m: the mempool
 * @ptr: a pointer to the target entry
 *
 * Returns an IOMAP address.
 */
static inline void *mempool_pagemem_to_iomap(struct mempool *m, void *ptr)
{
	return (void *) ((uintptr_t) ptr + m->iomap_offset);
}

extern int
mempool_pagemem_create(struct mempool *m, int nr_elems, size_t elem_len);
extern int mempool_pagemem_map_to_user(struct mempool *m);
extern void mempool_pagemem_destroy(struct mempool *m);

#endif /* __KERNEL__ */

