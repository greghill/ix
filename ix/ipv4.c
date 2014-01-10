/*
 * ipv4.c - Ethernet + IP Version 4 Support
 */

#include <stdio.h>

#include <ix/stddef.h>
#include <ix/byteorder.h>
#include <ix/timer.h>
#include <net/ethernet.h>
#include <net/arp.h>

#include "net.h"

static void ipv4_dump_eth_pkt(struct eth_hdr *hdr)
{
	struct eth_addr *dmac = &hdr->dhost;
	struct eth_addr *smac = &hdr->shost;
	printf("ipv4: dst MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
	       dmac->addr[0], dmac->addr[1], dmac->addr[2],
	       dmac->addr[3], dmac->addr[4], dmac->addr[5]);
	printf("ipv4: src MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
	       smac->addr[0], smac->addr[1], smac->addr[2],
	       smac->addr[3], smac->addr[4], smac->addr[5]);
	printf("ipv4: frame type: %x\n", ntoh16(hdr->type));
}

static void ipv4_rx_eth_pkt(struct rte_mbuf *pkt)
{
	struct eth_hdr *ethhdr = rte_pktmbuf_mtod(pkt, struct eth_hdr *);

	switch (ntoh16(ethhdr->type)) {
	case ETHTYPE_IP:
		rte_pktmbuf_free(pkt);
		break;
	case ETHTYPE_ARP:
		arp_process_pkt(pkt, next_hdr(ethhdr, struct arp_hdr *));
		break;
	default:
		rte_pktmbuf_free(pkt);
	}
}

void ipv4_rx_pkts(int n, struct rte_mbuf *pktv[])
{
	int i;

	timer_run();

	for (i = 0; i < n; i++) {
		ipv4_rx_eth_pkt(pktv[i]);
	}
}

