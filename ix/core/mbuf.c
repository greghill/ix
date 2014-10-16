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

/* Capacity should be at least RX queues per CPU * ETH_DEV_RX_QUEUE_SZ */
#define MBUF_CAPACITY	131072

DEFINE_PERCPU(struct mempool, mbuf_mempool);

void mbuf_default_done(struct mbuf *m)
{
	mbuf_free(m);
}

/**
 * mbuf_init_cpu - allocates the core-local mbuf region
 *
 * Returns 0 if successful, otherwise failure.
 */
int mbuf_init_cpu(void)
{
	int ret;
	struct mempool *m = &percpu_get(mbuf_mempool);

	BUILD_ASSERT(sizeof(struct mbuf) <= MBUF_HEADER_LEN);

	ret = mempool_pagemem_create(m, MBUF_CAPACITY, MBUF_LEN,MEMPOOL_SANITY_PERCPU,percpu_get(cpu_id));
	if (ret)
		return ret;

	ret = mempool_pagemem_map_to_user(m);
	if (ret) {
		mempool_pagemem_destroy(m);
		return ret;
	}

	return 0;
}

/**
 * mbuf_exit_cpu - frees the core-local mbuf region
 */
void mbuf_exit_cpu(void)
{
	mempool_pagemem_destroy(&percpu_get(mbuf_mempool));
}

