/*
 * ethfg.h - Support for flow groups, the basic unit of load balancing
 */

#pragma once

#include <ix/stddef.h>
#include <ix/list.h>
#include <ix/lock.h>
#include <ix/cpu.h>

#define ETH_MAX_NUM_FG	128 

#define NETHDEV	16
#define ETH_MAX_TOTAL_FG (ETH_MAX_NUM_FG * NETHDEV)

struct eth_rx_queue;

struct eth_fg {
	bool		in_transition;	/* is the fg being migrated? */
	unsigned int	cur_cpu;	/* the current CPU */
	unsigned int	target_cpu;	/* the migration target CPU */
	unsigned int	idx;		/* the flow index */

	spinlock_t	lock;		/* protects fg data during migration */

	void *		perfg;		/* per-flowgroup variables */

	void (*steer) (struct eth_rx_queue *target);

	struct		rte_eth_dev *eth;
};

struct eth_fg_listener {
	void (*prepare)	(struct eth_fg *fg);	/* called before a migration */
	void (*finish)	(struct eth_fg *fg);	/* called after a migration */
	struct list_node n;
};

extern void eth_fg_register_listener(struct eth_fg_listener *l);
extern void eth_fg_unregister_listener(struct eth_fg_listener *l);

DECLARE_PERCPU(void *, fg_offset);

/* used to define per-flowgroup variables */
#define DEFINE_PERFG(type, name) \
	typeof(type) perfg__##name __attribute__((section(".perfg,\"\",@nobits#")))

/* used to make per-flowgroup variables externally defined */
#define DECLARE_PERFG(type, name) \
	extern DEFINE_PERFG(type, name)

static inline void * __perfg_get(void *key)
{
        void *offset = percpu_get(fg_offset);

        return (void *) ((uintptr_t) key + (uintptr_t) offset);
}

/**
 * perfg_get - get the perqueue variable of the current processed queue
 * @var: the perqueue variable
 *
 * Returns a perqueue variable.
 */
#define perfg_get(var)                                               \
        (*((typeof(perfg__##var) *) (__perfg_get(&perfg__##var))))

/**
 * eth_fg_set_current - sets the current flowgroup
 * @fg: the flow group
 *
 * Per-flowgroup memory references are made relative to the
 * current flowgroup.
 */
static inline void eth_fg_set_current(struct eth_fg *fg)
{
	percpu_get(fg_offset) = fg->perfg;
}

static inline void unset_current_fg(void)
{
	//percpu_get(fg_offset) = NULL;  FIXME EdB THIS BREAKS ASSERTIONS
}


extern void eth_fg_init(struct eth_fg *fg, unsigned int idx);
extern int eth_fg_init_cpu(struct eth_fg *fg);
extern void eth_fg_free(struct eth_fg *fg);
extern void eth_fg_assign_to_cpu(int fg_id, int cpu);

extern int nr_flow_groups;

extern struct eth_fg *fgs[ETH_MAX_TOTAL_FG];

DECLARE_PERFG(int, dev_idx);

DECLARE_PERFG(int, fg_id);
