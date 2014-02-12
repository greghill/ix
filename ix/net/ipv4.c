/*
 * ipv4.c - Ethernet + IP Version 4 Support
 */

#include <stdio.h>

#include <ix/stddef.h>
#include <ix/byteorder.h>
#include <ix/timer.h>

#include <net/ethernet.h>
#include <net/ip.h>

#include "net.h"
#include "nic.h"

/**
 * ip_addr_to_str - prints an IP address as a human-readable string
 * @addr: the ip address
 * @str: a buffer to store the string
 *
 * The buffer must be IP_ADDR_STR_LEN in size.
 */
void ip_addr_to_str(struct ip_addr *addr, char *str)
{
	snprintf(str, IP_ADDR_STR_LEN, "%d.%d.%d.%d",
		 ((addr->addr >> 24) & 0xff),
                 ((addr->addr >> 16) & 0xff),
                 ((addr->addr >> 8) & 0xff),
                 (addr->addr & 0xff));
}

static void ipv4_ip_input(struct rte_mbuf *pkt, struct ip_hdr *hdr)
{
	int hdrlen, pktlen;

	/* check that the packet is long enough */
	if (!enough_space(pkt, hdr))
		goto out;
	/* check for IP version 4 */
	if (hdr->version != 4)
		goto out;
	/* the minimum legal IPv4 header length is 20 bytes (5 words) */
	if (hdr->header_len < 5)
		goto out;

	/* drop all IP fragment packets (unsupported) */
	if (ntoh16(hdr->off) & (IP_OFFMASK | IP_MF))
		goto out;

	hdrlen = hdr->header_len * sizeof(uint32_t);
	pktlen = ntoh16(hdr->len);

	/* FIXME: make sure pktlen isn't greater than the buffer size */
	/* the ip total length must be large enough to hold the header */
	if (pktlen < hdrlen)
		goto out;

	pktlen -= hdrlen;

	switch(hdr->proto) {
	case IPPROTO_ICMP:
		icmp_input(pkt,
			   next_hdr_off(hdr, struct icmp_hdr *, hdrlen),
			   pktlen);
		return;
	}

out:
	nic_ops->free_pkt(pkt);
}

static void ipv4_rx_eth_pkt(struct rte_mbuf *pkt)
{
	struct eth_hdr *ethhdr = rte_pktmbuf_mtod(pkt, struct eth_hdr *);

	switch (ntoh16(ethhdr->type)) {
	case ETHTYPE_IP:
		ipv4_ip_input(pkt, next_hdr(ethhdr, struct ip_hdr *));
		break;
	case ETHTYPE_ARP:
		arp_input(pkt, next_hdr(ethhdr, struct arp_hdr *));
		break;
	default:
		nic_ops->free_pkt(pkt);
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

