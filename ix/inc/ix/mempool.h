/*
 * mempool.h - a fast fixed-sized memory pool allocator
 */

#pragma once

#include <ix/stddef.h>

struct mempool_hdr {
	struct mempool_hdr *next;
} __packed;

struct mempool {
	void *buf;
	struct mempool_hdr *head;
	size_t len;
	int count;
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
 *
 * NOTE: Must be the same memory pool that it was allocated from
 */
static inline void mempool_free(struct mempool *m, void *ptr)
{
	struct mempool_hdr *elem = (struct mempool_hdr *) ptr;

	elem->next = m->head;
	m->head = elem;
}

extern int mempool_create(struct mempool *m, int count, size_t len);
extern void mempool_destroy(struct mempool *m);

