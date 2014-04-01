#include <ix/mbuf.h>
#include <ix/timer.h>

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

struct ip_globals ip_data;

extern void tcp_tmr(void);
extern struct tcp_pcb *tcp_active_pcbs;
extern struct tcp_pcb *tcp_tw_pcbs;
/* in us */
#define TCP_TMR_INTERVAL 250000

static struct timer tcp_timer;
static int tcpip_tcp_timer_active;

static void tcpip_tcp_timer(struct timer *t)
{
	/* call TCP timer handler */
	tcp_tmr();
	/* timer still needed? */
	if (tcp_active_pcbs || tcp_tw_pcbs) {
		/* restart timer */
		timer_add(&tcp_timer, TCP_TMR_INTERVAL);
	} else {
		/* disable timer */
		tcpip_tcp_timer_active = 0;
	}
}

void tcp_timer_needed(void)
{
	/* timer is off but needed again? */
	if (!tcpip_tcp_timer_active && (tcp_active_pcbs || tcp_tw_pcbs)) {
		/* enable and start timer */
		tcpip_tcp_timer_active = 1;
		timer_init_entry(&tcp_timer, &tcpip_tcp_timer);
		timer_add(&tcp_timer, TCP_TMR_INTERVAL);
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
	ip_data.current_iphdr_dest.addr = iphdr->dst_addr.addr;
	ip_data.current_iphdr_src.addr = iphdr->src_addr.addr;
	tcp_input(pbuf, &netif);
	mbuf_free(pkt);
}

DEFINE_PERCPU(struct mempool, pbuf_mempool);
DEFINE_PERCPU(struct mempool, pbuf_with_payload_mempool);
DEFINE_PERCPU(struct mempool, tcp_pcb_mempool);
DEFINE_PERCPU(struct mempool, tcp_pcb_listen_mempool);
DEFINE_PERCPU(struct mempool, tcp_seg_mempool);

#define PBUF_WITH_PAYLOAD_SIZE 4096

static int init_mempool(struct mempool *m, int nr_elems, size_t elem_len)
{
	int ret;

	ret = mempool_pagemem_create(m, nr_elems, elem_len);
	if (ret)
		return ret;

	ret = mempool_pagemem_map_to_user(m);
        if (ret) {
                mempool_pagemem_destroy(m);
                return ret;
        }

        return 0;
}

int memp_init(void)
{
	if (init_mempool(&percpu_get(pbuf_mempool), 65536, memp_sizes[MEMP_PBUF]))
		return 1;

	if (init_mempool(&percpu_get(pbuf_with_payload_mempool), 65536, PBUF_WITH_PAYLOAD_SIZE))
		return 1;

	if (init_mempool(&percpu_get(tcp_pcb_mempool), 65536, memp_sizes[MEMP_TCP_PCB]))
		return 1;

	if (init_mempool(&percpu_get(tcp_pcb_listen_mempool), 4, memp_sizes[MEMP_TCP_PCB_LISTEN]))
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
