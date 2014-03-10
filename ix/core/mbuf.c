/*
 * mbuf.c - buffer management for network packets
 *
 * TODO: add support for mapping into user-level address space...
 */

#include <ix/stddef.h>
#include <ix/mem.h>
#include <ix/mempool.h>
#include <ix/mbuf.h>
#include <ix/cpu.h>

#define MBUF_CAPACITY	4096

DEFINE_PERCPU(struct mempool, mbuf_mempool);

/**
 * mbuf_init_cpu - allocates the core-local mbuf region
 *
 * Returns 0 if successful, otherwise failure.
 */
int mbuf_init_cpu(void)
{
	BUILD_ASSERT(sizeof(struct mbuf) <= MBUF_HEADER_LEN);

	return mempool_create_phys(&percpu_get(mbuf_mempool),
		MBUF_CAPACITY, MBUF_LEN);
}

/**
 * mbuf_exit_cpu - frees the core-local mbuf region
 */
void mbuf_exit_cpu(void)
{
	mempool_destroy(&percpu_get(mbuf_mempool));
}

