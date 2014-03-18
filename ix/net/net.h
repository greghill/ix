/*
 * net.h - the network stack local header
 */

#pragma once

#include <ix/types.h>
#include <ix/mbuf.h>
#include <ix/ethdev.h>

#include <net/arp.h>
#include <net/icmp.h>
#include <net/udp.h>

/* Address Resolution Protocol (ARP) definitions */
extern int arp_lookup_mac(struct ip_addr *addr, struct eth_addr *mac);
extern void arp_input(struct mbuf *pkt, struct arp_hdr *hdr);
extern int arp_init(void);

/* Internet Control Message Protocol (ICMP) definitions */
extern void icmp_input(struct mbuf *pkt, struct icmp_hdr *hdr, int len);

/* Unreliable Datagram Protocol (UDP) definitions */
extern void udp_input(struct mbuf *pkt, struct ip_hdr *iphdr,
		      struct udp_hdr *udphdr);
extern void udp_init(void);

/* Transmission Control Protocol (TCP) definitions */
/* FIXME: change when we integrate better with LWIP */
extern void tcp_input_tmp(struct mbuf *pkt, struct ip_hdr *iphdr, void *tcphdr);

/**
 * ip_setup_header - outputs a typical IP header
 * @iphdr: a pointer to the header
 * @proto: the protocol
 * @saddr: the source address
 * @daddr: the destination address
 * @l4len: the length of the L4 (e.g. UDP or TCP) header and data.
 */
static inline void ip_setup_header(struct ip_hdr *iphdr, uint8_t proto,
				   uint32_t saddr, uint32_t daddr,
				   uint16_t l4len)
{
	iphdr->header_len = sizeof(struct ip_hdr) / 4;
	iphdr->version = 4;
	iphdr->tos = 0;
	iphdr->len = hton16(sizeof(struct ip_hdr) + l4len);
	iphdr->id = 0;
	iphdr->off = 0;
	iphdr->ttl = 64;
	iphdr->proto = proto;
	iphdr->chksum = 0;
	iphdr->src_addr.addr = hton32(saddr);
	iphdr->dst_addr.addr = hton32(daddr);
}

