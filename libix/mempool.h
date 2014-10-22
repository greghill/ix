/*
 * mempool.h - a fast fixed-sized memory pool allocator
 */

#pragma once

#include <ix/stddef.h>
#include <ix/mem.h>
#include <assert.h>

#ifdef __KERNEL__
#error "wrong include ... this is userlevel mempool"
#endif /* __KERNEL__ */


#define MEMPOOL_INITIAL_OFFSET (0)

struct mempool_hdr {
	struct mempool_hdr *next;
} __packed;

struct mempool {
	uint64_t                magic;
	void			*buf;
	struct mempool_hdr	*head;
	int			nr_pages;
	uint32_t                nr_elems;
	size_t                  elem_len;
	int                     page_aligned;
};
#define MEMPOOL_MAGIC   0x12911776

/**
 * mempool_alloc - allocates an element from a memory pool
 * @m: the memory pool
 *
 * Returns a pointer to the allocated element or NULL if unsuccessful.
 */
static inline void *mempool_alloc(struct mempool *m)
{
	struct mempool_hdr *h = m->head;

	if (likely(h)) {
		m->head = h->next;
	}
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

static inline void *mempool_idx_to_ptr(struct mempool *m, uint32_t idx)
{
	void *p;
	assert(idx<m->nr_elems);
	assert(!m->page_aligned);
	p = m->buf + m->elem_len*idx + MEMPOOL_INITIAL_OFFSET;
	return p;
}

static inline uintptr_t mempool_ptr_to_idx(struct mempool *m,void *p)
{
	uintptr_t x = (uintptr_t)p - (uintptr_t)m->buf - MEMPOOL_INITIAL_OFFSET;
	x = x / m->elem_len;
	assert (x<m->nr_elems);
	return x;
}


extern int mempool_create(struct mempool *m, int nr_elems, size_t elem_len);
extern void mempool_destroy(struct mempool *m);



