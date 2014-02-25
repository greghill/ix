/*
 * udp.c - Unreliable Datagram Protocol (UDP) Support
 */

#define DEBUG 1

#include <ix/stddef.h>
#include <ix/byteorder.h>
#include <ix/log.h>

#include <net/ip.h>
#include <net/udp.h>

#include "net.h"

void udp_input(struct mbuf *pkt, struct ip_hdr *iphdr, struct udp_hdr *udphdr)
{
	struct ip_addr addr;
	char src[IP_ADDR_STR_LEN];
	char dst[IP_ADDR_STR_LEN];

	addr.addr = ntoh32(iphdr->src_addr.addr);
	ip_addr_to_str(&addr, src);
	addr.addr = ntoh32(iphdr->dst_addr.addr);
	ip_addr_to_str(&addr, dst);

	log_debug("udp: got UDP packet from '%s' to '%s', source port %d, dest port %d, len %d\n",
		  src, dst, ntoh16(udphdr->src_port), ntoh16(udphdr->dst_port),
		  ntoh16(udphdr->len));
	mbuf_free(pkt);
}

