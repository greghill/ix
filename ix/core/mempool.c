/*
 * mempool.c - A fast per-CPU memory pool implementation
 *
 * A mempool is a heap that only supports allocations of a
 * single fixed size. Generally, mempools are created up front
 * with non-trivial setup cost and then used frequently with
 * very low marginal allocation cost.
 * 
 * Mempools are not thread-safe. This allows them to be really
 * efficient but you must provide your own locking if you intend
 * to use a memory pool with more than one core. Mempools also
 * have NUMA affinity to the core they were created for, so
 * using them from the wrong core frequently could result in
 * poor performance.
 *
 * The basic implementation strategy is to partition a region of
 * memory into several fixed-sized slots and to maintain a
 * singly-linked free list of available slots. Objects are
 * allocated in LIFO order so that preference is given to the
 * free slots that are most cache hot.
 */

#include <ix/stddef.h>
#include <ix/errno.h>
#include <ix/mempool.h>
#include <ix/page.h>
#include <ix/vm.h>

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
int mempool_create(struct mempool *m, int nr_elems, size_t elem_len)
{
	int nr_pages;

	if (!elem_len || !nr_elems)
		return -EINVAL;

	elem_len = align_up(elem_len, sizeof(long));
	nr_pages = PGN_2MB(nr_elems * elem_len + PGMASK_2MB);
	nr_elems = nr_pages * PGSIZE_2MB / elem_len;
	m->nr_pages = nr_pages;

	m->buf = mem_alloc_pages(nr_pages, PGSIZE_2MB, NULL, MPOL_PREFERRED);
	if (m->buf == MAP_FAILED)
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
	mem_free_pages(m->buf, m->nr_pages, PGSIZE_2MB);
	m->buf = NULL;
	m->head = NULL;
}

static void
mempool_init_buf_with_pages(struct mempool *m, int elems_per_page,
			    int nr_pages, size_t elem_len)
{
	int i, j;
	struct mempool_hdr *cur, *prev = (struct mempool_hdr *) &m->head;

	for (i = 0; i < nr_pages; i++) {
			cur = (struct mempool_hdr *)
				((uintptr_t) m->buf + i * PGSIZE_2MB);
		for (j = 0; j < elems_per_page; j++) {
			prev->next = cur;
			prev = cur;
			cur = (struct mempool_hdr *)
				((uintptr_t) cur + elem_len);
		}
	}

	prev->next = NULL;
}

/**
 * mempool_pagemem_create - initializes a memory pool with page memory
 * @m: the memory pool
 * @nr_elems: the minimum number of elements
 * @elem_len: the length of each element
 *
 * Use this variant if you need objects to not cross page boundaries or
 * want to map the memory at user-level. Otherwise, mempool_create() is
 * a better option because it has more efficient element packing.
 *
 * NOTE: Just like mempool_create(), mempool_pagemem_create() will always
 * create at least @nr_elems elements, but it may create more depending on
 * page alignment.
 *
 * Returns 0 if successful, otherwise fail.
 */
int mempool_pagemem_create(struct mempool *m, int nr_elems, size_t elem_len)
{
	int nr_pages, elems_per_page;

	if (!elem_len || elem_len > PGSIZE_2MB || !nr_elems)
		return -EINVAL;

	elem_len = align_up(elem_len, sizeof(long));
	elems_per_page = PGSIZE_2MB / elem_len;
	nr_pages = div_up(nr_elems, elems_per_page);
	m->nr_pages = nr_pages;

	m->buf = page_alloc_contig(nr_pages);
	if (!m->buf)
		return -ENOMEM;

	mempool_init_buf_with_pages(m, elems_per_page, nr_pages, elem_len);

	return 0;
}

/**
 * mempool_pagemem_map_to_user - make the memory pool available to user memory
 * @m: the memory pool.
 *
 * NOTE: we map the memory read-only.
 *
 * Returns 0 if successful, otherwise fail.
 */
int mempool_pagemem_map_to_user(struct mempool *m)
{
	m->iomap_addr = vm_map_to_user(m->buf, m->nr_pages,
				       PGSIZE_2MB, VM_PERM_R);
	if (!m->iomap_addr)
		return -ENOMEM;

	m->iomap_offset = (uintptr_t) m->iomap_addr - (uintptr_t) m->buf;

	return 0;
}

/**
 * mempool_pagemem_destroy - destroys a memory pool allocated with page memory
 * @m: the memory pool
 */
void mempool_pagemem_destroy(struct mempool *m)
{
	if (m->iomap_addr) {
		vm_unmap(m->iomap_addr, m->nr_pages, PGSIZE_2MB);
		m->iomap_addr = NULL;
	}

        page_free_contig(m->buf, m->nr_pages);
        m->buf = NULL;
        m->head = NULL;
}

