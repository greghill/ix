#include <unistd.h>
#include <sys/syscall.h>

#include <ix/errno.h>
#include <ix/ethdev.h>
#include <ix/mem.h>
#include <ix/queue.h>
#include <ix/stddef.h>

extern const char __perqueue_start[];
extern const char __perqueue_end[];

DEFINE_PERCPU(void *, current_perqueue);
DEFINE_PERQUEUE(int, queue_id);

static void * queue_init_perqueue(unsigned int numa_node)
{
	size_t len = __perqueue_end - __perqueue_start;
	char *addr;

	addr = mem_alloc_pages_onnode(div_up(len, PGSIZE_2MB),
				      PGSIZE_2MB, numa_node, MPOL_BIND);
	if (!addr)
		return NULL;

	memset(addr, 0, len);

	return addr;
}

/**
 * queue_init_one - initializes a queue
 * @queue: the queue number
 *
 * Returns 0 if successful, otherwise fail.
 */
int queue_init_one(struct eth_rx_queue *rx_queue)
{
	void *pqueue;

	pqueue = queue_init_perqueue(percpu_get(cpu_numa_node));
	if (!pqueue)
		return -ENOMEM;

	rx_queue->perqueue_offset = pqueue;

	return 0;
}

void set_current_queue(struct eth_rx_queue *rx_queue)
{
	percpu_get(current_perqueue) = rx_queue->perqueue_offset;
}

int set_current_queue_by_index(unsigned int index)
{
#if 0
       if (index >= percpu_get(eth_rx_count)) {
               unset_current_queue();
               return 1;
       }

       set_current_queue(percpu_get(eth_rx)[index]);
#endif

	if (index > 0)
		return 1;

	set_current_queue(percpu_get(eth_rx));
	return 0;
}

void unset_current_queue()
{
	percpu_get(current_perqueue) = NULL;
}

