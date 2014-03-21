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

/* TODO: implement */
void tcp_timer_needed(void)
{
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
}
