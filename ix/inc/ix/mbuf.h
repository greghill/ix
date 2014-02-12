/*
 * mbuf.h - buffer management for network packets
 */

#pragma once

#include <ix/stddef.h>
#include <ix/mem.h>
#include <ix/mempool.h>

struct mbuf {
	struct mempool *pool;	/* the pool the mbuf was allocated from */
	physaddr_t paddr;	/* the physical address of the mbuf data */
	size_t len;		/* the length of the mbuf data */
	struct mbuf *next;	/* the next buffer of the packet
				 * (can happen with recieve-side coalescing) */

	/* offload features */
	union {
		uint32_t rss;
		struct {
			uint16_t hash;
			uint16_t id;
		} fdir;
		uint32_t sched;
	} hash;
	uint32_t ol_flags;
};

#define MBUF_HEADER_LEN		64	/* one cache line */
#define MBUF_DATA_LEN		2048	/* 2 kb */
//BUILD_ASSERT(sizeof(struct mbuf) <= MBUF_HEADER_LEN);

/**
 * mbuf_mtod_off - cast a pointer to the data with an offset
 * @m: the mbuf
 * @type: the type to cast
 * @off: the offset
 */
#define mbuf_mtod_off(m, type, off) \
	((type) ((uintptr_t) (m) + MBUF_HEADER_LEN + (off)))

/**
 * mbuf_mtod - cast a pointer to the beginning of the data
 * @m: the mbuf
 * @type: the type to cast
 */
#define mbuf_mtod(m, type) ((type) \
	((uintptr_t) (m) + MBUF_HEADER_LEN))

/**
 * mbuf_nextd_off - advance a data pointer by an offset
 * @d: the starting data pointer
 * @type: the new type to cast
 * @off: the offset
 */
#define mbuf_nextd_off(d, type, off) \
	((type) ((uintptr_t) (d) + (off)))

/**
 * mbuf_nextd - advance a data pointer to the end of the current type
 * @d: the starting data pointer
 * @type: the new type to cast
 *
 * Automatically infers the size of the starting data structure.
 */
#define mbuf_nextd(d, type) \
	mbuf_nextd_off(d, type, sizeof(typeof(*buf)))

/**
 * mbuf_alloc - allocate an mbuf from a memory pool
 * @pool: the memory pool
 *
 * Returns an mbuf, or NULL if failure.
 */
static inline struct mbuf *mbuf_alloc(struct mempool *pool)
{
	struct mbuf *m = mempool_alloc(pool);
	if (unlikely(!m))
		return NULL;

	m->pool = pool;
	m->paddr = mempool_get_phys(pool, mbuf_mtod(m, void *));
	m->next = NULL;

	return m;
}

/**
 * mbuf_free - frees an mbuf
 * @m: the mbuf
 */
static inline void mbuf_free(struct mbuf *m)
{
	mempool_free(m->pool, m);
}

