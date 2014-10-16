/*
 * mempool.c - A fast memory pool implementation
 */

#include <ix/stddef.h>
#include <ix/mem.h>
#include <ix/mempool.h>

#include <errno.h>

#include "ix.h"

static void mempool_init_buf(struct mempool *m, int nr_elems, size_t elem_len)
{
	int i;
	uintptr_t pos = (uintptr_t) m->buf;
	uintptr_t next_pos = pos + elem_len;

	m->head = (struct mempool_hdr *)pos;

	for (i = 0; i < nr_elems - 1; i++) {
		((struct mempool_hdr *)pos)->next =
			(struct mempool_hdr *)next_pos;

		pos = next_pos;
		next_pos += elem_len;
	}

	((struct mempool_hdr *)pos)->next = NULL;
}

/**
 * mempool_create - initializes a memory pool
 * @nr_elems: the minimum number of elements in the pool
 * @elem_len: the length of each element
 *
 * NOTE: mempool_create() will create a pool with at least @nr_elems,
 * but possibily more depending on page alignment.
 *
 * Returns 0 if successful, otherwise fail.
 */
int mempool_create(struct mempool *m, int nr_elems, size_t elem_len, int16_t foo, int16_t bar)
{
	int nr_pages;

	if (!elem_len || !nr_elems)
		return -EINVAL;

	elem_len = align_up(elem_len, sizeof(long)) + sizeof(void*);
	nr_pages = PGN_2MB(nr_elems * elem_len + PGMASK_2MB);
	nr_elems = nr_pages * PGSIZE_2MB / elem_len;
	m->nr_pages = nr_pages;

	m->buf = ix_alloc_pages(nr_pages);
	if (!m->buf)
		return -ENOMEM;

	mempool_init_buf(m, nr_elems, elem_len);
	return 0;
}

/**
 * mempool_destroy - cleans up a memory pool and frees memory
 * @m: the memory pool
 */
void mempool_destroy(struct mempool *m)
{
	ix_free_pages(m->buf, m->nr_pages);
	m->buf = NULL;
	m->head = NULL;
}

