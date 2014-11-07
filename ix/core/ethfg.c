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

#define TRANSITION_TIMEOUT (100 * ONE_MS)

extern const char __perfg_start[];
extern const char __perfg_end[];

DEFINE_PERCPU(void *, fg_offset);

int nr_flow_groups;

struct eth_fg *fgs[ETH_MAX_TOTAL_FG];

DEFINE_PERFG(int, dev_idx);

DEFINE_PERFG(int, fg_id);

struct queue {
	struct mbuf *head;
	struct mbuf *tail;
};

DEFINE_PERCPU(struct queue, local_mbuf_queue);
DEFINE_PERCPU(struct queue, remote_mbuf_queue);
DEFINE_PERCPU(struct hlist_head, remote_timers_list);
DEFINE_PERCPU(uint64_t, remote_timer_pos);

static void transition_handler_prev(struct timer *t);
static void transition_handler_target(void *fg_);
static void migrate_pkts_to_remote(struct eth_fg *fg);
static void migrate_timers_to_remote(int fg_id);
static void migrate_timers_from_remote();

/**
 * eth_fg_init - initializes a flow group globally
 * @fg: the flow group
 * @idx: the index number of the flow group
 */
void eth_fg_init(struct eth_fg *fg, unsigned int idx)
{
	fg->perfg = NULL;
	fg->idx = idx;
	fg->cur_cpu = -1;
	spin_lock_init(&fg->lock);
	timer_init_entry(&fg->transition_timeout, transition_handler_prev);
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

	assert(!fg->in_transition);

	if (fg->cur_cpu == cfg_cpu[cpu]) {
		percpu_get(cp_cmd)->status = CP_STATUS_READY;
		return;
	} else if (fg->cur_cpu == -1) {
		fg->cur_cpu = cfg_cpu[cpu];
	} else {
		fg->in_transition = true;
		fg->prev_cpu = fg->cur_cpu;
		fg->cur_cpu = -1;
		fg->target_cpu = cfg_cpu[cpu];
		timer_percpu_add(&fg->transition_timeout, TRANSITION_TIMEOUT);
		migrate_pkts_to_remote(fg);
		migrate_timers_to_remote(fg_id);
	}

	rss_reta.mask_lo = 0;
	rss_reta.mask_hi = 0;
	if (fg->idx >= 64)
		rss_reta.mask_hi = ((uint64_t) 1) << (fg->idx - 64);
	else
		rss_reta.mask_lo = ((uint64_t) 1) << fg->idx;

	rss_reta.reta[fg->idx] = cpu;
	cp_shmem->flow_group[fg_id].cpu = cpu;
	fg->eth->dev_ops->reta_update(fg->eth, &rss_reta);
}

static void transition_handler_prev(struct timer *t)
{
	struct eth_fg *fg = container_of(t, struct eth_fg, transition_timeout);

	cpu_run_on_one(transition_handler_target, fg, fg->target_cpu);
}

static struct eth_rx_queue * queue_from_fg(struct eth_fg *fg)
{
        int i;
        struct eth_rx_queue *rxq;

        for (i = 0; i < percpu_get(eth_num_queues); i++) {
                rxq = percpu_get(eth_rxqs[i]);
                if (fg->eth == rxq->dev)
			return rxq;
        }

	assert(false);
}

static void transition_handler_target(void *fg_)
{
	struct mbuf *pkt;
	struct queue *q;
	struct eth_fg *fg = (struct eth_fg *)fg_;
	int prev_cpu = fg->prev_cpu;

	fg->in_transition = false;
	fg->cur_cpu = fg->target_cpu;
	fg->target_cpu = -1;
	fg->prev_cpu = -1;

	q = &percpu_get(remote_mbuf_queue);
	pkt = q->head;
	while (pkt) {
		eth_input(queue_from_fg(fg), pkt);
		pkt = pkt->next;
	}
	q->head = NULL;
	q->tail = NULL;

	q = &percpu_get(local_mbuf_queue);
	pkt = q->head;
	while (pkt) {
		eth_input(queue_from_fg(fg), pkt);
		pkt = pkt->next;
	}
	q->head = NULL;
	q->tail = NULL;

	migrate_timers_from_remote();

	percpu_get_remote(cp_cmd, prev_cpu)->status = CP_STATUS_READY;
}

static void migrate_pkts_to_remote(struct eth_fg *fg)
{
	struct eth_rx_queue *rxq = queue_from_fg(fg);
	struct mbuf *pkt = rxq->head;
	struct mbuf **prv = &rxq->head;
	struct queue *q = &percpu_get_remote(remote_mbuf_queue, fg->target_cpu);

	while (pkt) {
		if (fg == &rxq->dev->data->rx_fgs[pkt->fg_id]) {
			*prv = pkt->next;

			pkt->next = NULL;
			if (!q->head) {
				q->head = pkt;
				q->tail = pkt;
			} else {
				q->tail->next = pkt;
				q->tail = pkt;
			}

			pkt = *prv;
		} else {
			prv = &pkt->next;
			pkt = pkt->next;
		}
	}
}

static void enqueue(struct queue *q, struct mbuf *pkt)
{
	pkt->next = NULL;
	if (!q->head) {
		q->head = pkt;
		q->tail = pkt;
	} else {
		q->tail->next = pkt;
		q->tail = pkt;
	}
}

void eth_recv_at_prev(struct eth_rx_queue *rx_queue, struct mbuf *pkt)
{
	struct eth_fg *fg = &rx_queue->dev->data->rx_fgs[pkt->fg_id];
	struct queue *q = &percpu_get_remote(remote_mbuf_queue, fg->target_cpu);
	enqueue(q, pkt);
}

void eth_recv_at_target(struct eth_rx_queue *rx_queue, struct mbuf *pkt)
{
	struct queue *q = &percpu_get(local_mbuf_queue);
	enqueue(q, pkt);
}

int eth_recv_handle_fg_transition(struct eth_rx_queue *rx_queue, struct mbuf *pkt)
{
	struct eth_fg *fg = &rx_queue->dev->data->rx_fgs[pkt->fg_id];

	if (!fg->in_transition && fg->cur_cpu == percpu_get(cpu_id)) {
		/* continue processing */
		return 0;
	} else if (fg->in_transition && fg->prev_cpu == percpu_get(cpu_id)) {
		eth_recv_at_prev(rx_queue, pkt);
		return 1;
	} else if (fg->in_transition && fg->target_cpu == percpu_get(cpu_id)) {
		eth_recv_at_target(rx_queue, pkt);
		return 1;
	} else {
		/* FIXME: somebody must mbuf_free(pkt) but we cannot do it here
		   because we don't own the memory pool */
		log_warn("dropping packet: flow group %d should be handled by cpu %d\n", perfg_get(fg_id), fgs[perfg_get(fg_id)]->cur_cpu);
		return 1;
	}
}

static void migrate_timers_to_remote(int fg_id)
{
	struct eth_fg *fg = fgs[fg_id];
	struct hlist_head *timers_list = &percpu_get_remote(remote_timers_list, fg->target_cpu);
	uint64_t *timer_pos = &percpu_get_remote(remote_timer_pos, fg->target_cpu);
	uint8_t fg_vector[ETH_MAX_TOTAL_FG] = {0};

	fg_vector[fg_id] = 1;
	hlist_init_head(timers_list);
	timer_collect_fgs(fg_vector, timers_list, timer_pos);
}

static void migrate_timers_from_remote()
{
	timer_reinject_fgs(&percpu_get(remote_timers_list), percpu_get(remote_timer_pos));
}
