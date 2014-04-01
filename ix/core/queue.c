#include <unistd.h>
#include <sys/syscall.h>

#include <ix/errno.h>
#include <ix/mem.h>
#include <ix/queue.h>
#include <ix/stddef.h>

void *perqueue_offsets[NQUEUE];

extern const char __perqueue_start[];
extern const char __perqueue_end[];

DEFINE_PERCPU(void *, current_perqueue);

static void * queue_init_perqueue(unsigned int queue, unsigned int numa_node)
{
	size_t len = __perqueue_end - __perqueue_start;
	char *addr;

	addr = mem_alloc_pages_onnode(div_up(len, PGSIZE_2MB),
				      PGSIZE_2MB, numa_node, MPOL_BIND);
	if (!addr)
		return NULL;

	memset(addr, 0, len);

	*((char **) addr) = addr;
	perqueue_offsets[queue] = addr;

	return addr;
}

/**
 * queue_init_one - initializes a queue
 * @queue: the queue number
 *
 * Returns 0 if successful, otherwise fail.
 */
int queue_init_one(unsigned int queue)
{
	int ret;
	unsigned int tmp, numa_node;
	void *pqueue;

	ret = syscall(SYS_getcpu, &tmp, &numa_node, NULL);
	if (ret)
		return -ENOSYS;

	pqueue = queue_init_perqueue(queue, numa_node);
	if (!pqueue)
		return -ENOMEM;

	return 0;
}

int set_current_queue(unsigned int queue)
{
	if (queue >= NQUEUE || !perqueue_offsets[queue]) {
		unset_current_queue();
		return 1;
	}

	percpu_get(current_perqueue) = perqueue_offsets[queue];

	return 0;
}

void unset_current_queue()
{
	percpu_get(current_perqueue) = NULL;
}
