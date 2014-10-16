#include <ix/mbuf.h>
#include <ix/timer.h>
#include <ix/queue.h>
#include <ix/ethfg.h>

#include <net/ip.h>

#include <lwip/memp.h>
#include <lwip/pbuf.h>

static struct netif {
	char unused[32];
} netif;

struct ip_globals
{
	char unused[20];
	struct ip_addr current_iphdr_src;
	struct ip_addr current_iphdr_dest;
};

void tcp_input(struct pbuf *p, struct netif *inp);

DEFINE_PERFG(struct ip_globals, ip_data);

extern void tcp_tmr(void);
DECLARE_PERFG(struct tcp_pcb *, tcp_active_pcbs);
DECLARE_PERFG(struct tcp_pcb *, tcp_tw_pcbs);
/* in us */
#define TCP_TMR_INTERVAL 250000

static DEFINE_PERCPU(struct timer, tcp_timer);
static DEFINE_PERFG(int, tcpip_tcp_timer_active);

static void tcpip_tcp_timer(struct timer *t)
{
	unsigned int queue;
	int needed;

	needed = 0;
	for_each_queue(queue) {
		if (!perfg_get(tcpip_tcp_timer_active))
			continue;

		/* call TCP timer handler */
		tcp_tmr();

		/* timer still needed? */
		if (perfg_get(tcp_active_pcbs) || perfg_get(tcp_tw_pcbs)) {
			/* restart timer */
			needed = 1;
		} else {
			/* disable timer */
			perfg_get(tcpip_tcp_timer_active) = 0;
		}
	}

	if (needed)
		timer_add(&percpu_get(tcp_timer), TCP_TMR_INTERVAL);
}

void tcp_timer_needed(void)
{
	/* timer is off but needed again? */
	if (!perfg_get(tcpip_tcp_timer_active) && (perfg_get(tcp_active_pcbs) || perfg_get(tcp_tw_pcbs))) {
		/* enable and start timer */
		perfg_get(tcpip_tcp_timer_active) = 1;
		if (!percpu_get(tcp_timer).handler)
			timer_init_entry(&percpu_get(tcp_timer), &tcpip_tcp_timer);

		if (!timer_pending(&percpu_get(tcp_timer)))
			timer_add(&percpu_get(tcp_timer), TCP_TMR_INTERVAL);
	}
}

struct netif *ip_route(struct ip_addr *dest)
{
	return &netif;
}

void tcp_input_tmp(struct mbuf *pkt, struct ip_hdr *iphdr, void *tcphdr)
{
	struct pbuf *pbuf;

	pbuf = pbuf_alloc(PBUF_RAW, ntoh16(iphdr->len) - iphdr->header_len * 4, PBUF_ROM);
	pbuf->payload = tcphdr;
	pbuf->mbuf = pkt;
	perfg_get(ip_data).current_iphdr_dest.addr = iphdr->dst_addr.addr;
	perfg_get(ip_data).current_iphdr_src.addr = iphdr->src_addr.addr;
	tcp_input(pbuf, &netif);
}

DEFINE_PERCPU(struct mempool, pbuf_mempool);
DEFINE_PERCPU(struct mempool, pbuf_with_payload_mempool);
DEFINE_PERCPU(struct mempool, tcp_pcb_mempool);
DEFINE_PERCPU(struct mempool, tcp_pcb_listen_mempool);
DEFINE_PERCPU(struct mempool, tcp_seg_mempool);

#define PBUF_WITH_PAYLOAD_SIZE 4096

static int init_mempool(struct mempool *m, int nr_elems, size_t elem_len)
{
	return mempool_create(m, nr_elems, elem_len, MEMPOOL_SANITY_PERCPU,percpu_get(cpu_id));
}

int memp_init(void)
{
	if (init_mempool(&percpu_get(pbuf_mempool), 65536, memp_sizes[MEMP_PBUF]))
		return 1;

	if (init_mempool(&percpu_get(pbuf_with_payload_mempool), 65536, PBUF_WITH_PAYLOAD_SIZE))
		return 1;

	if (init_mempool(&percpu_get(tcp_pcb_mempool), 65536, memp_sizes[MEMP_TCP_PCB]))
		return 1;

	if (init_mempool(&percpu_get(tcp_pcb_listen_mempool), 65536, memp_sizes[MEMP_TCP_PCB_LISTEN]))
		return 1;

	if (init_mempool(&percpu_get(tcp_seg_mempool), 65536, memp_sizes[MEMP_TCP_SEG]))
		return 1;

	return 0;
}

void *mem_malloc(size_t size)
{
	LWIP_ASSERT("mem_malloc", size <= PBUF_WITH_PAYLOAD_SIZE);

	return mempool_alloc(&percpu_get(pbuf_with_payload_mempool));
}

void mem_free(void *ptr)
{
	mempool_free(&percpu_get(pbuf_with_payload_mempool), ptr);
}
