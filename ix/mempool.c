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
 *
 * FIXME: Eventually we will need to make service calls to the
 * control plane in order to allocate memory pages. For now we
 * make calls to Linux directly as a temporary work-around. The
 * control plane API will need to be more sophisticated (e.g.
 * NUMA-awareness).
 */

#include <ix/stddef.h>
#include <ix/errno.h>
#include <ix/mempool.h>
#include <sys/mman.h>

static void mempool_init_buf(struct mempool *m, int count, size_t len)
{
	int i;
	uintptr_t pos = (uintptr_t) m->buf;
	uintptr_t next_pos = pos + len;

	m->head = (struct mempool_hdr *)pos;

	for (i = 0; i < count - 1; i++) {
		((struct mempool_hdr *)pos)->next =
			(struct mempool_hdr *)next_pos;

		pos = next_pos;
		next_pos += len;
	}

	((struct mempool_hdr *)next_pos)->next = NULL;
}

int mempool_create(struct mempool *m, int count, size_t len)
{
	if (len < sizeof(struct mempool_hdr) || !count)
		return -EINVAL;

	m->buf = mmap(NULL, count * len, PROT_READ | PROT_WRITE,
		      MAP_HUGETLB | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (m->buf == MAP_FAILED)
		return -ENOMEM;

	m->count = count;
	m->len = len;

	mempool_init_buf(m, count, len);
	return 0;
}

void mempool_destroy(struct mempool *m)
{
	munmap(m->buf, m->count * m->len);
	m->buf = NULL;
	m->head = NULL;
}

