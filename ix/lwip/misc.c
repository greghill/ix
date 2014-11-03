#include <ix/mbuf.h>
#include <ix/timer.h>
#include <ix/queue.h>
#include <ix/ethfg.h>

#include <net/ip.h>

#include <lwip/memp.h>
#include <lwip/pbuf.h>
#include <ix/ethfg.h>

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

DEFINE_PERCPU(struct ip_globals, ip_data);

extern void tcp_tmr(void);
DECLARE_PERFG(struct tcp_pcb *, tcp_active_pcbs);
DECLARE_PERFG(struct tcp_pcb *, tcp_tw_pcbs);
/* in us */
#define TCP_TMR_INTERVAL 250000

static DEFINE_PERFG(struct timer, tcp_timer);
static DEFINE_PERFG(int, tcpip_tcp_timer_active);

static void tcpip_tcp_timer(struct timer *t)
{
	int needed;
	needed = 0;
	
	assert(t == &perfg_get(tcp_timer));
	assert(perfg_get(tcpip_tcp_timer_active));
	if (!perfg_get(tcpip_tcp_timer_active))
		return;
	
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
	
	if (needed)
		timer_add(&perfg_get(tcp_timer), TCP_TMR_INTERVAL);
}

void tcp_timer_needed(void)
{
	/* timer is off but needed again? */
	if (!perfg_get(tcpip_tcp_timer_active) && (perfg_get(tcp_active_pcbs) || perfg_get(tcp_tw_pcbs))) {
		/* enable and start timer */
		perfg_get(tcpip_tcp_timer_active) = 1;
		if (!perfg_get(tcp_timer).handler)
			timer_init_entry(&perfg_get(tcp_timer), &tcpip_tcp_timer);

		if (!timer_pending(&perfg_get(tcp_timer)))
			timer_add(&perfg_get(tcp_timer), TCP_TMR_INTERVAL);
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
	percpu_get(ip_data).current_iphdr_dest.addr = iphdr->dst_addr.addr;
	percpu_get(ip_data).current_iphdr_src.addr = iphdr->src_addr.addr;
	tcp_input(pbuf, &netif);
}


static struct mempool_datastore  pbuf_ds;
static struct mempool_datastore  pbuf_with_payload_ds;
static struct mempool_datastore  tcp_pcb_ds;
static struct mempool_datastore  tcp_pcb_listen_ds;
static struct mempool_datastore  tcp_seg_ds;

DEFINE_PERCPU(struct mempool, pbuf_mempool);
DEFINE_PERCPU(struct mempool, pbuf_with_payload_mempool);
DEFINE_PERCPU(struct mempool, tcp_pcb_mempool);
DEFINE_PERCPU(struct mempool, tcp_pcb_listen_mempool);
DEFINE_PERCPU(struct mempool, tcp_seg_mempool);

#define MEMP_SIZE (64*1024)

#define PBUF_WITH_PAYLOAD_SIZE 4096

static int init_mempool(struct mempool_datastore *m, int nr_elems, size_t elem_len, const char *prettyname)
{
	return mempool_create_datastore(m, nr_elems, elem_len, 0, MEMPOOL_DEFAULT_CHUNKSIZE,prettyname);
}

int memp_init(void)
{
	if (init_mempool(&pbuf_ds, MEMP_SIZE, memp_sizes[MEMP_PBUF],"pbuf"))
		return 1;

	if (init_mempool(&pbuf_with_payload_ds, MEMP_SIZE, PBUF_WITH_PAYLOAD_SIZE,"pbuf_payload"))
		return 1;

	if (init_mempool(&tcp_pcb_ds, MEMP_SIZE, memp_sizes[MEMP_TCP_PCB],"tcp_pcb"))
		return 1;

	if (init_mempool(&tcp_pcb_listen_ds, MEMP_SIZE, memp_sizes[MEMP_TCP_PCB_LISTEN],"tcp_pcb_listen"))
		return 1;

	if (init_mempool(&tcp_seg_ds, MEMP_SIZE, memp_sizes[MEMP_TCP_SEG],"tcp_seg"))
		return 1;
	return 0;
}

int memp_init_cpu(void)
{
	int cpu = percpu_get(cpu_id);
	if (mempool_create(&percpu_get(pbuf_mempool),&pbuf_ds,MEMPOOL_SANITY_PERCPU, cpu))
		return 1;

	if (mempool_create(&percpu_get(pbuf_with_payload_mempool), &pbuf_with_payload_ds, MEMPOOL_SANITY_PERCPU, cpu))
		return 1;

	if (mempool_create(&percpu_get(tcp_pcb_mempool), &tcp_pcb_ds, MEMPOOL_SANITY_PERCPU, cpu))
		return 1;

	if (mempool_create(&percpu_get(tcp_pcb_listen_mempool), &tcp_pcb_listen_ds, MEMPOOL_SANITY_PERCPU, cpu))
		return 1;

	if (mempool_create(&percpu_get(tcp_seg_mempool), &tcp_seg_ds, MEMPOOL_SANITY_PERCPU, cpu))
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
