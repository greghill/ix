/*
 * mempool.h - a fast fixed-sized memory pool allocator
 */

#pragma once

#include <ix/stddef.h>
#include <ix/mem.h>
#include <assert.h>
#include <ix/cpu.h>
#include <ix/queue.h>
#include <ix/ethfg.h>


#ifdef __KERNEL__
// FIXME:offset can be made conditional to KSTATS
#define MEMPOOL_INITIAL_OFFSET (sizeof(void*))
#else
#define MEMPOOL_INITIAL_OFFSET (0)
#endif

struct mempool_hdr {
	struct mempool_hdr *next;
} __packed;

struct mempool {
	uint64_t                magic;
	void			*buf;
	struct mempool_hdr	*head;
	int			nr_pages;
	int                     sanity;
	uint32_t                nr_elems;
	size_t                  elem_len;
	int                     page_aligned;
#ifdef __KERNEL__
	void 			*iomap_addr;
	uintptr_t		iomap_offset;
#endif
};
#define MEMPOOL_MAGIC   0x12911776


/*
 * mempool sanity macros ensures that pointers between mempool-allocated objects are of identical type
 */

#define MEMPOOL_SANITY_GLOBAL    0
#define MEMPOOL_SANITY_PERCPU    1
#define MEMPOOL_SANITY_PERQUEUE  2
#define MEMPOOL_SANITY_PERFG     3

#ifdef ENABLE_KSTATS


#define MEMPOOL_SANITY_OBJECT(_a) do {\
	struct mempool **hidden = (struct mempool **)_a;\
	assert(hidden[-1] && hidden[-1]->magic == MEMPOOL_MAGIC); \
	} while (0);

static inline int __mempool_get_sanity(void *a) {
	struct mempool *p = (struct mempool *)(((uint64_t) a))-MEMPOOL_INITIAL_OFFSET;
	return p->sanity;
}

#define MEMPOOL_SANITY_ISPERFG(_a) assert(__mempool_get_sanity(_a)>>16 == MEMPOOL_SANITY_PERFG)

#define MEMPOOL_SANITY_ACCESS(_obj)   do { \
	MEMPOOL_SANITY_OBJECT(_obj);\
	int sanity = __mempool_get_sanity(_obj); \
	if ((sanity >>16)==MEMPOOL_SANITY_PERCPU) assert(percpu_get(cpu_id)==(sanity & 0xffff));\
	if ((sanity >>16)==MEMPOOL_SANITY_PERQUEUE) assert(perqueue_get(queue_id)==(sanity &0xffff));\
	if ((sanity >>16)==MEMPOOL_SANITY_PERFG) assert(perfg_get(fg_id)==(sanity &0xffff));\
	} while (0);

#define  MEMPOOL_SANITY_LINK(_a,_b) do {\
	MEMPOOL_SANITY_ACCESS(_a);\
	MEMPOOL_SANITY_ACCESS(_b);\
	int sa = __mempool_get_sanity(_a);\
	int sb = __mempool_get_sanity(_b);\
	assert (sa == sb);\
	}  while (0);

#else
#define MEMPOOL_SANITY_ISPERFG(_a)
#define MEMPOOL_SANITY_ACCESS(_a)
#define MEMPOOL_SANITY_LINK(_a,_b)
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
	MEMPOOL_SANITY_ACCESS(ptr);
	elem->next = m->head;
	m->head = elem;
}

static inline void *mempool_idx_to_ptr(struct mempool *m, uint32_t idx)
{
	void *p;
	assert(idx<m->nr_elems);
	assert(!m->page_aligned);
	p = m->buf + m->elem_len*idx + MEMPOOL_INITIAL_OFFSET;
	MEMPOOL_SANITY_ACCESS(p);
	return p;
}

static inline uintptr_t mempool_ptr_to_idx(struct mempool *m,void *p)
{
	uintptr_t x = (uintptr_t)p - (uintptr_t)m->buf - MEMPOOL_INITIAL_OFFSET;
	x = x / m->elem_len;
	assert (x<m->nr_elems);
	return x;
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
mempool_pagemem_create(struct mempool *m, int nr_elems, size_t elem_len,int16_t sanity_type, int16_t sanity_id);
extern int mempool_pagemem_map_to_user(struct mempool *m);
extern void mempool_pagemem_destroy(struct mempool *m);

#endif /* __KERNEL__ */

