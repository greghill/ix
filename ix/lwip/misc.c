#include <ix/mbuf.h>

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

void tcp_timer_needed(void)
{
	printf("TODO: implement tcp_timer_needed\n");
}

struct netif *ip_route(struct ip_addr *dest)
{
	return &netif;
}

void tcp_input_tmp(struct mbuf *pkt, struct ip_hdr *iphdr, void *tcphdr)
{
	struct pbuf *pbuf;

	pbuf = pbuf_alloc(PBUF_RAW, pkt->len - (tcphdr - mbuf_mtod(pkt, void*)), PBUF_ROM);
	pbuf->payload = tcphdr;
	ip_data.current_iphdr_dest.addr = iphdr->dst_addr.addr;
	ip_data.current_iphdr_src.addr = iphdr->src_addr.addr;
	tcp_input(pbuf, &netif);
}
