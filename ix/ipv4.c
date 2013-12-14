/*
 * ipv4.c - Ethernet + IP Version 4 Support
 */

#include <stdio.h>

#include <ix/stddef.h>
#include <ix/byteorder.h>
#include <net/ethernet.h>

#include <rte_config.h>
#include <rte_mbuf.h>

static void ipv4_dump_eth_pkt(struct ether_hdr *hdr)
{
	struct ether_addr *dmac = &hdr->dhost;
	struct ether_addr *smac = &hdr->shost;
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
	struct ether_hdr *ethhdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

	ipv4_dump_eth_pkt(ethhdr);
}

void ipv4_rx_pkts(int n, struct rte_mbuf *pktv[])
{
	int i;

	for (i = 0; i < n; i++) {
		ipv4_rx_eth_pkt(pktv[i]);
		rte_pktmbuf_free(pktv[i]);
	}
}

