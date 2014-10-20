/*
 * control_plane.h - control plane definitions
 */

#include <ix/compiler.h>
#include <ix/queue.h>

struct queue_metrics {
	volatile int depth;
} __aligned(64);

struct flow_group_metrics {
	volatile int depth;
} __aligned(64);

extern volatile struct cp_shmem {
	struct queue_metrics queue[NQUEUE];
	struct flow_group_metrics flow_group[128];
} *cp_shmem;

static inline void cp_queue_depth(int queue_id, int depth)
{
	cp_shmem->queue[queue_id].depth = depth;
}

static inline void cp_flow_group_depth(int flow_group_id, int diff)
{
	cp_shmem->flow_group[flow_group_id].depth += diff;
}
