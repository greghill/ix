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

#define MEMPOOL_DEFAULT_CHUNKSIZE 128
#define MEMPOOL_INITIAL_OFFSET (0)

struct mempool_hdr {
	struct mempool_hdr *next;
	struct mempool_hdr *next_chunk;
} __packed;


// one per data type
struct mempool_datastore {
	uint64_t                 magic;
	spinlock_t               lock;
	struct mempool_hdr      *chunk_head;
	void			*buf;
	int			nr_pages;
	uint32_t                nr_elems;
	size_t                  elem_len;
	int                     nostraddle;
	int                     chunk_size;
	int                     num_chunks;
	int                     free_chunks;
	int64_t                 num_locks;
	const char             *prettyname;
	struct mempool_datastore *next_ds;
};

struct mempool {
	uint64_t                 magic;
	void			*buf;
	struct mempool_datastore *datastore;
	struct mempool_hdr	*head;
	struct mempool_hdr      *private_chunk;
//	int			nr_pages;
	uint32_t                nr_elems;
	size_t                  elem_len;
	int                     nostraddle;
	int                     chunk_size;
	int                     num_alloc;
	int                     num_free;
};
#define MEMPOOL_MAGIC   0x12911776

/**
 * mempool_alloc - allocates an element from a memory pool
 * @m: the memory pool
 *
 * Returns a pointer to the allocated element or NULL if unsuccessful.
 */
extern void *mempool_alloc_2(struct mempool *m);
static inline void *mempool_alloc(struct mempool *m)
{
	struct mempool_hdr *h = m->head;

	if (likely(h)) {
		m->head = h->next;
		m->num_alloc++;
		m->num_free--;
		return (void *) h;
	} else {
		return mempool_alloc_2(m);
	}
}

/**
 * mempool_free - frees an element back in to a memory pool
 * @m: the memory pool
 * @ptr: the element
 *
 * NOTE: Must be the same memory pool that it was allocated from
 */
extern void mempool_free_2(struct mempool *m, void *ptr);
static inline void mempool_free(struct mempool *m, void *ptr)
{
	struct mempool_hdr *elem = (struct mempool_hdr *) ptr;

	if (likely(m->num_free<m->chunk_size)) {
		m->num_free++;
		m->num_alloc--;
		elem->next = m->head;
		m->head = elem;	
	} else 
		mempool_free_2(m,ptr);
}

extern int mempool_create_datastore(struct mempool_datastore *m, int nr_elems, size_t elem_len, int nostraddle, int chunk_size, const char *prettyname);
extern int mempool_create(struct mempool *m, struct mempool_datastore *mds);
extern void mempool_destroy(struct mempool *m);
