/*
 * ethfg.c - Support for flow groups, the basic unit of load balancing
 */

#include <ix/stddef.h>
#include <ix/ethfg.h>
#include <ix/mem.h>
#include <ix/errno.h>
#include <ix/ethdev.h>
#include <ix/control_plane.h>
#include <ix/cfg.h>

extern const char __perfg_start[];
extern const char __perfg_end[];

DEFINE_PERCPU(void *, fg_offset);

int nr_flow_groups;

struct eth_fg *fgs[ETH_MAX_TOTAL_FG];

DEFINE_PERFG(int, dev_idx);

DEFINE_PERFG(int, fg_id);

/**
 * eth_fg_init - initializes a flow group globally
 * @fg: the flow group
 * @idx: the index number of the flow group
 */
void eth_fg_init(struct eth_fg *fg, unsigned int idx)
{
	fg->perfg = NULL;
	fg->idx = idx;
	spin_lock_init(&fg->lock);
}

/**
 * eth_fg_init_cpu - intialize a flow group for a specific CPU
 * @fg: the flow group
 *
 * Returns 0 if successful, otherwise fail.
 */
int eth_fg_init_cpu(struct eth_fg *fg)
{
	size_t len = __perfg_end - __perfg_start;
	char *addr;

	addr = mem_alloc_pages_onnode(div_up(len, PGSIZE_2MB),
				      PGSIZE_2MB, percpu_get(cpu_numa_node),
				      MPOL_BIND);
	if (!addr)
		return -ENOMEM;

	memset(addr, 0, len);
	fg->perfg = addr;

	return 0;
}

/**
 * eth_fg_free - frees all memory used by a flow group
 * @fg: the flow group
 */
void eth_fg_free(struct eth_fg *fg)
{
	size_t len = __perfg_end - __perfg_start;

	if (fg->perfg)
		mem_free_pages(fg->perfg, div_up(len, PGSIZE_2MB), PGSIZE_2MB);
}

/**
 * eth_fg_assign_to_cpu - assigns the flow group to the given cpu
 * @fg_id: the flow group (global name; across devices)
 * @cpu: the cpu sequence number
 */
void eth_fg_assign_to_cpu(int fg_id, int cpu)
{
	struct rte_eth_rss_reta rss_reta;
	struct eth_fg *fg = fgs[fg_id];

	rss_reta.mask_lo = 0;
	rss_reta.mask_hi = 0;
	if (fg->idx >= 64)
		rss_reta.mask_hi = ((uint64_t) 1) << (fg->idx - 64);
	else
		rss_reta.mask_lo = ((uint64_t) 1) << fg->idx;

	rss_reta.reta[fg->idx] = cpu;
	cp_shmem->flow_group[fg_id].cpu = cpu;
	fg->cur_cpu = cfg_cpu[cpu];
	fg->eth->dev_ops->reta_update(fg->eth, &rss_reta);
}
