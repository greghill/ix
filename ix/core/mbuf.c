/*
 * mbuf.c - buffer management for network packets
 *
 * TODO: make per-cpu...
 * TODO: add support for mapping into user-level address space...
 */

#include <ix/stddef.h>
#include <ix/mem.h>
#include <ix/mempool.h>
#include <ix/mbuf.h>

#define MBUF_CAPACITY	4096

struct mempool mbuf_mempool;

/**
 * mbuf_init_core - allocates the core-local mbuf region
 *
 * Returns 0 if successful, otherwise failure.
 */
int mbuf_init_core(void)
{
	BUILD_ASSERT(sizeof(struct mbuf) <= MBUF_HEADER_LEN);

	return mempool_create_phys(&mbuf_mempool, MBUF_CAPACITY, MBUF_LEN);
}

/**
 * mbuf_exit_core - frees the core-local mbuf region
 */
void mbuf_exit_core(void)
{
	mempool_destroy(&mbuf_mempool);
}

