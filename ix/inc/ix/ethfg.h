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
#define TCP_ACTIVE_PCBS_MAX_BUCKETS (512)    // per flow group

//FIXME - should be a function of max_cpu * NETDEV
#define NQUEUE 64

struct eth_rx_queue;


struct tcp_hash_entry { 
	struct hlist_head pcbs;
	struct hlist_node hash_link;
};

struct eth_fg {
	uint16_t        fg_id;          /* self */
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

	// LWIP/TCP globals (per flow)
	struct timer          tcpip_timer;
	bool                  tcp_active_pcb_changed;
	bool                  tcp_timer;
	bool                  tcp_timer_ctr;

	uint32_t              iss;
	uint32_t              tcp_ticks;
	struct hlist_head     active_buckets; // tcp_hash_entry list
	struct hlist_head     tw_pcbs;        // tcp_pcb
	struct hlist_head     bound_pcbs;     // tcp_pcb
	struct tcp_hash_entry active_tbl[TCP_ACTIVE_PCBS_MAX_BUCKETS];

};

struct eth_fg_listener {
	void (*prepare)	(struct eth_fg *fg);	/* called before a migration */
	void (*finish)	(struct eth_fg *fg);	/* called after a migration */
	struct list_node n;
};

extern void eth_fg_register_listener(struct eth_fg_listener *l);
extern void eth_fg_unregister_listener(struct eth_fg_listener *l);

DECLARE_PERCPU(struct eth_fg *,the_cur_fg); // ugly - avoid using


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
	percpu_get(the_cur_fg) = fg;
}

static inline void unset_current_fg(void)
{
	percpu_get(the_cur_fg) = NULL;
}

static inline int perfg_exists(void)
{
	return percpu_get(the_cur_fg) != NULL;
}

extern void eth_fg_init(struct eth_fg *fg, unsigned int idx);
extern int eth_fg_init_cpu(struct eth_fg *fg);
extern void eth_fg_free(struct eth_fg *fg);
extern void eth_fg_assign_to_cpu(bitmap_ptr fg_bitmap, int cpu);

extern int nr_flow_groups;

extern struct eth_fg *fgs[ETH_MAX_TOTAL_FG];


static inline struct eth_fg *get_ethfg_from_id(int fg_id)
{
	if (fg_id<0) 
		return NULL;
	else 
		return fgs[fg_id];
}

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
