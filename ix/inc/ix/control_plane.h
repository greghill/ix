/*
 * control_plane.h - control plane definitions
 */

#include <ix/compiler.h>
#include <ix/queue.h>

struct queue_metrics {
	volatile int depth;
} __aligned(64);

extern struct cp_shmem {
	struct queue_metrics queue[NQUEUE];
} *cp_shmem;

static inline void cp_queue_depth(int queue_id, int depth)
{
	cp_shmem->queue[queue_id].depth = depth;
}
