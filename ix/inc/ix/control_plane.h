/*
 * control_plane.h - control plane definitions
 */

#include <ix/compiler.h>
#include <ix/ethfg.h>

#define IDLE_FIFO_SIZE 256

struct cpu_metrics {
	double queuing_delay;
	double batch_size;
} __aligned(64);

struct flow_group_metrics {
	int cpu;
} __aligned(64);

enum commands {
	CP_CMD_NOP = 0,
	CP_CMD_MIGRATE,
	CP_CMD_IDLE,
};

enum status {
	CP_STATUS_READY = 0,
	CP_STATUS_RUNNING,
};

struct command_struct {
	enum commands cmd_id;
	enum status status;
	union {
		struct {
			DEFINE_BITMAP(fg_bitmap, ETH_MAX_TOTAL_FG);
			int cpu;
		} migrate;
		struct {
			char fifo[IDLE_FIFO_SIZE];
		} idle;
	};
};

extern volatile struct cp_shmem {
	uint32_t nr_flow_groups;
	uint32_t nr_cpus;
	struct cpu_metrics cpu_metrics[NCPU];
	struct flow_group_metrics flow_group[ETH_MAX_TOTAL_FG];
	struct command_struct command[NCPU];
} *cp_shmem;

DECLARE_PERCPU(volatile struct command_struct *, cp_cmd);

void cp_idle(void);

static inline double ema_update(double prv_value, double value, double alpha)
{
	return alpha * value + (1 - alpha) * prv_value;
}

#define EMA_UPDATE(ema, value, alpha) ema = ema_update(ema, value, alpha)
