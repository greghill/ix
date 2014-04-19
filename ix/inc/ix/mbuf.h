/*
 * mbuf.h - buffer management for network packets
 */

#pragma once

#include <ix/stddef.h>
#include <ix/mem.h>
#include <ix/mempool.h>
#include <ix/cpu.h>
#include <ix/page.h>
#include <ix/syscall.h>

struct mbuf_iov {
	void *base;
	machaddr_t maddr;
	size_t len;
};

/**
 * mbuf_iov_create - creates an mbuf IOV and references the IOV memory
 * @iov: the IOV to create
 * @ent: the reference sg entry
 *
 * Returns the length of the mbuf IOV (could be less than the sg entry).
 */
static inline size_t
mbuf_iov_create(struct mbuf_iov *iov, struct sg_entry *ent)
{
	size_t len = min(ent->len, PGSIZE_2MB - PGOFF_2MB(ent->base));

	iov->base = ent->base;
	iov->maddr = page_get(ent->base);
	iov->len = len;

	return len;
}

/**
 * mbuf_iov_free - unreferences the IOV memory
 * @iov: the IOV
 */
static inline void mbuf_iov_free(struct mbuf_iov *iov)
{
	page_put(iov->base);
}

struct mbuf {
	struct mempool *pool;	/* the pool the mbuf was allocated from */
	size_t len;		/* the length of the mbuf data */
	struct mbuf *next;	/* the next buffer of the packet
				 * (can happen with recieve-side coalescing) */
	struct mbuf_iov *iovs;	/* transmit scatter-gather array */
	unsigned int nr_iov;	/* the number of scatter-gather vectors */

	void (*done) (struct mbuf *m);
	unsigned long done_data;
	uint16_t ol_flags;	/* offload flags */
};

#define MBUF_HEADER_LEN		64	/* one cache line */
#define MBUF_DATA_LEN		2048	/* 2 kb */
#define MBUF_LEN		(MBUF_HEADER_LEN + MBUF_DATA_LEN)

/* Offload flag bits */
#define PKT_TX_IP_CKSUM      0x1000 /**< IP cksum of TX pkt. computed by NIC. */
#define PKT_TX_TCP_CKSUM     0x2000 /**< TCP cksum of TX pkt. computed by NIC. */


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
 * mbuf_to_iomap - determines the address in the IOMAP region
 * @mbuf: the mbuf
 * @pos: a pointer within the mbuf
 *
 * Returns an address.
 */
#define mbuf_to_iomap(mbuf, pos) \
	((void *) ((uintptr_t) (pos) + (mbuf)->pool->iomap_offset))

/**
 * iomap_to_mbuf - determines the mbuf pointer based on the IOMAP address
 * @pool: the containing memory pool
 * @pos: the IOMAP pointer
 *
 * Returns an address.
 */
#define iomap_to_mbuf(pool, pos) \
	((void *) ((uintptr_t) (pos) - (pool)->iomap_offset))

extern void mbuf_default_done(struct mbuf *m);

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
	m->next = NULL;
	m->done = &mbuf_default_done;

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
 * mbuf_get_data_machaddr - get the machine address of the mbuf data
 * @m: the mbuf
 *
 * Returns a machine address.
 */
static inline machaddr_t mbuf_get_data_machaddr(struct mbuf *m)
{
	return page_machaddr(mbuf_mtod(m, void *));
}

/**
 * mbuf_xmit_done - called when a TX queue completes an mbuf
 * @m: the mbuf
 */
static inline void mbuf_xmit_done(struct mbuf *m)
{
	m->done(m);
}

DECLARE_PERCPU(struct mempool, mbuf_mempool);

/**
 * mbuf_alloc_local - allocate an mbuf from the core-local mempool
 *
 * Returns an mbuf, or NULL if out of memory.
 */
static inline struct mbuf *mbuf_alloc_local(void)
{
	return mbuf_alloc(&percpu_get(mbuf_mempool));
}

extern int mbuf_init_cpu(void);
extern void mbuf_exit_cpu(void);

/*
 * direct dispatches into network stack
 * FIXME: add a function for each entry point (e.g. udp and tcp)
 */
struct eth_rx_queue;

extern void eth_input(struct eth_rx_queue *rx_queue, struct mbuf *pkt);

