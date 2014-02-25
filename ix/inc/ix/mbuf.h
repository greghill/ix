/*
 * mbuf.h - buffer management for network packets
 */

#pragma once

#include <ix/stddef.h>
#include <ix/mem.h>
#include <ix/mempool.h>

struct mbuf_iov {
	machaddr_t base;
	size_t len;
};

struct mbuf {
	struct mempool *pool;	/* the pool the mbuf was allocated from */
	machaddr_t maddr;	/* the machine address of the mbuf data */
	size_t len;		/* the length of the mbuf data */
	struct mbuf *next;	/* the next buffer of the packet
				 * (can happen with recieve-side coalescing) */
	struct mbuf_iov *iovs;	/* transmit scatter-gather array */
	int nr_iov;		/* the number of scatter-gather vectors */
};

#define MBUF_HEADER_LEN		64	/* one cache line */
#define MBUF_DATA_LEN		2048	/* 2 kb */
#define MBUF_LEN		(MBUF_HEADER_LEN + MBUF_DATA_LEN)

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
 * @mbuf: the mbuf
 * @type: the type to cast
 */
#define mbuf_mtod(mbuf, type) \
	((type) ((uintptr_t) (mbuf) + MBUF_HEADER_LEN))

/**
 * mbuf_nextd_off - advance a data pointer by an offset
 * @ptr: the starting data pointer
 * @type: the new type to cast
 * @off: the offset
 */
#define mbuf_nextd_off(ptr, type, off) \
	((type) ((uintptr_t) (ptr) + (off)))

/**
 * mbuf_nextd - advance a data pointer to the end of the current type
 * @ptr: the starting data pointer
 * @type: the new type to cast
 *
 * Automatically infers the size of the starting data structure.
 */
#define mbuf_nextd(ptr, type) \
	mbuf_nextd_off(ptr, type, sizeof(typeof(*ptr)))

/**
 * mbuf_enough_space - determines if the buffer is large enough
 * @mbuf: the mbuf
 * @pos: a pointer to the current position in the mbuf data
 * @sz: the length to go past the current position
 *
 * Returns true if there is room, otherwise false.
 */
#define mbuf_enough_space(mbuf, pos, sz) \
	((uintptr_t) (pos) - ((uintptr_t) (mbuf) + MBUF_HEADER_LEN) + (sz) <= \
	 (mbuf)->len)

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
	m->maddr = mempool_get_mach(pool, mbuf_mtod(m, void *));
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

/**
 * mbuf_xmit_done - called when a TX queue completes an mbuf
 * @m: the mbuf
 */
static inline void mbuf_xmit_done(struct mbuf *m)
{
	/* FIXME: will need to propogate up to user application */
	mbuf_free(m);
}

extern struct mempool mbuf_mempool;

extern int mbuf_init_core(void);
extern void mbuf_exit_core(void);

/*
 * direct dispatches into network stack
 * FIXME: add a function for each entry point (e.g. udp and tcp)
 */
extern void eth_input(struct mbuf *pkt);

