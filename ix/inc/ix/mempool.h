/*
 * mempool.h - a fast fixed-sized memory pool allocator
 */

#pragma once

#include <ix/stddef.h>
#include <ix/mem.h>

struct mempool_hdr {
	struct mempool     *pool;
	struct mempool_hdr *next;
} __packed;

struct mempool {
	void			*buf;
	struct mempool_hdr	*head;
	int			nr_pages;
	int                     sanity;

#ifdef __KERNEL__
	void 			*iomap_addr;
	uintptr_t		iomap_offset;
#endif
};


/*
 * mempool sanity macros ensures that pointers between mempool-allocated objects are of identical type
 */

#define MEMPOOL_SANITY_GLOBAL    0
#define MEMPOOL_SANITY_PERCPU    1
#define MEMPOOL_SANITY_PERQUEUE  2
#define MEMPOOL_SANITY_PERFG     3

#ifdef ENABLE_KSTATS

#define MEMPOOL_SANITY_ISPERFG(_a) assert((_a)->pool->sanity >>16 == MEMPOOL_SANITY_PERFG)


#define MEMPOOL_SANITY_ACCESS(_a) do {\
	if (((_a)->pool->sanity >>16)==MEMPOOL_SANITY_PERCPU) assert((percpu_get(cpu_id))==((_a)->pool->sanity &0xffff)); \
	if (((_a)->pool->sanity >>16)==MEMPOOL_SANITY_PERQUEUE) assert((perqueue_get(queue_id))==((_a)->pool->sanity &0xffff)); \
	if (((_a)->pool->sanity >>16)==MEMPOOL_SANITY_PERFG) assert((perfg_get(fg_id))==((_a)->pool->sanity &0xffff)); \
} while (0);

#define MEMPOOL_SANITY_LINK(_a,_b) do {\
	if ((_b) != NULL) assert((_a)->pool->sanity == (_b)->pool->sanity); \
	MEMPOOL_SANITY_ACCESS(_a);\
} while (0);

#else
#define MEMPOOL_SANITY_ACCESS(_a)
#define MEMPOOL_SANITY_CHECK(_a,_b)
#endif



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
		h->pool = m;
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

extern int mempool_create(struct mempool *m, int nr_elems, size_t elem_len, int16_t sanity_type, int16_t sanity_id);
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

