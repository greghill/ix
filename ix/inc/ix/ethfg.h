/*
 * ethfg.h - Support for flow groups, the basic unit of load balancing
 */

#pragma once

#include <ix/stddef.h>
#include <ix/list.h>
#include <ix/lock.h>
#include <ix/cpu.h>
#include <assert.h>
#include <ix/timer.h>
#include <ix/bitmap.h>

#define ETH_MAX_NUM_FG	128 

#define NETHDEV	16
#define ETH_MAX_TOTAL_FG (ETH_MAX_NUM_FG * NETHDEV)

//FIXME - should be a function of max_cpu * NETDEV
#define NQUEUE 64

struct eth_rx_queue;

struct eth_fg {
	bool		in_transition;	/* is the fg being migrated? */
	unsigned int	cur_cpu;	/* the current CPU */
	unsigned int	target_cpu;	/* the migration target CPU */
	unsigned int	prev_cpu;
	unsigned int	idx;		/* the flow index */
	unsigned int	dev_idx;

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
	assert(fg->cur_cpu == percpu_get(cpu_id));
	percpu_get(fg_offset) = fg->perfg;
}

static inline void unset_current_fg(void)
{
	percpu_get(fg_offset) = NULL;
}

static inline int perfg_exists(void)
{
	return percpu_get(fg_offset) != NULL;
}

extern void eth_fg_init(struct eth_fg *fg, unsigned int idx);
extern int eth_fg_init_cpu(struct eth_fg *fg);
extern void eth_fg_free(struct eth_fg *fg);
extern void eth_fg_assign_to_cpu(bitmap_ptr fg_bitmap, int cpu);

extern int nr_flow_groups;

extern struct eth_fg *fgs[ETH_MAX_TOTAL_FG];

DECLARE_PERFG(int, dev_idx);

DECLARE_PERFG(int, fg_id);


/**                                                                                                                                                                         
 * for_each_active_fg -- iterates over all fg owned by this cpu
 * @fgid: integer to store the fg
 *                                                                                                                                                                           */

static inline unsigned int __fg_next_active(unsigned int fgid)
{
        while (fgid < nr_flow_groups) {
                fgid++;

                if (fgs[fgid]->cur_cpu == percpu_get(cpu_id))
                        return fgid;
        }

        return fgid;
}

#define for_each_active_fg(fgid)                                \
        for ((fgid) = -1; (fgid) = __fgid_next_active(fgid); (fgid) < nr_flow_groups)

DECLARE_PERCPU(unsigned int, cpu_numa_node);

struct mbuf;

int eth_recv_handle_fg_transition(struct eth_rx_queue *rx_queue, struct mbuf *pkt);
