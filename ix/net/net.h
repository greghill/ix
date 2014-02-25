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

