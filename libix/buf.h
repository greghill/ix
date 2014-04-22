/*
 * buf.h = transmit data buffer management
 */

#pragma once

#include <ix/stddef.h>
#include <ix/mempool.h>

#include <pthread.h>
#include <string.h>


#define IX_MTU	1460

extern __thread struct mempool ixev_buf_pool;

struct ixev_buf {
	uint32_t len;
	uint32_t pad;
	char payload[IX_MTU];
};

/**
 * ixev_buf_alloc - allocates a buffer
 *
 * The initial refcount is set to one.
 *
 * Returns a buffer, or NULL if out of memory.
 */
static inline struct ixev_buf *ixev_buf_alloc(void)
{
	struct ixev_buf *buf = mempool_alloc(&ixev_buf_pool);

	if (unlikely(!buf))
		return NULL;

	buf->len = 0;

	return buf;
}

/**
 * ixev_buf_store - store data inside a buffer
 * @buf: the buffer
 * @addr: the start address of the data
 * @len: the length of the data
 *
 * Returns the numbers of bytes successfully stored in the buffer,
 * or zero if the buffer is full.
 */
static inline size_t ixev_buf_store(struct ixev_buf *buf, void *addr, size_t len)
{
	size_t avail = min(len, IX_MTU - buf->len);

	if (!avail)
		return 0;

	memcpy(&buf->payload[buf->len], addr, avail);
	buf->len += avail;

	return avail;
}

/**
 * ixev_is_buf_full - determines if the buffer is full
 * @buf: the buffer
 *
 * Returns true if the buffer is full, otherwise false.
 */
static inline bool ixev_is_buf_full(struct ixev_buf *buf)
{
	return buf->len == IX_MTU;
}

/**
 * ixev_buf_put - decrements the refcount of a buffer
 * @buf: the buffer
 *
 * Frees the buffer if the refcount reaches zero.
 */
static inline void ixev_buf_release(struct ixev_buf *buf)
{
	mempool_free(&ixev_buf_pool, buf);
}

