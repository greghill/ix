/*
 * control_plane.h - control plane definitions
 */

#include <ix/compiler.h>
#include <ix/queue.h>
#include <ix/ethfg.h>

struct queue_metrics {
	volatile int depth;
} __aligned(64);

struct flow_group_metrics {
	volatile int depth;
	int cpu;
} __aligned(64);

enum commands {
	CP_CMD_NOP = 0,
	CP_CMD_MIGRATE_FLOW_GROUP,
};

struct command_struct {
	enum commands cmd_id;
	union {
		struct {
			int flow;
			int cpu;
		} migrate_flow_group;
	};
};

extern volatile struct cp_shmem {
	struct queue_metrics queue[NQUEUE];
	struct flow_group_metrics flow_group[ETH_MAX_TOTAL_FG];
	struct command_struct command[NCPU];
} *cp_shmem;

DECLARE_PERCPU(volatile struct command_struct *, cp_cmd);

static inline void cp_queue_depth(int queue_id, int depth)
{
	cp_shmem->queue[queue_id].depth = depth;
}

static inline void cp_flow_group_depth(int flow_group_id, int diff)
{
	cp_shmem->flow_group[flow_group_id].depth += diff;
}
