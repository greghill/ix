#include <ix/mbuf.h>
#include <ix/timer.h>

#include <net/ip.h>

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
