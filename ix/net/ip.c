/*
 * ip.c - Ethernet + IP Version 4 Support
 */

#include <stdio.h>

#include <ix/stddef.h>
#include <ix/byteorder.h>
#include <ix/errno.h>
#include <ix/log.h>
#include <ix/cfg.h>
#include <ix/control_plane.h>

#include <asm/chksum.h>

#include <net/ethernet.h>
#include <net/ip.h>

/* FIXME: remove when we integrate better with LWIP */
#include <lwip/pbuf.h>
#include "net.h"

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

static void ip_input(struct eth_fg *cur_fg, struct mbuf *pkt, struct ip_hdr *hdr)
{
	int hdrlen, pktlen;

	/* check that the packet is long enough */
	if (!mbuf_enough_space(pkt, hdr, sizeof(struct ip_hdr)))
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

	/* the ip total length must be large enough to hold the header */
	if (pktlen < hdrlen)
		goto out;
	if (!mbuf_enough_space(pkt, hdr, pktlen))
		goto out;

	pktlen -= hdrlen;

	switch(hdr->proto) {
	case IPPROTO_TCP:
		/* FIXME: change when we integrate better with LWIP */
		tcp_input_tmp(cur_fg, pkt, hdr, mbuf_nextd_off(hdr, void *, hdrlen));
		break;
	case IPPROTO_UDP:
		udp_input(pkt, hdr,
			  mbuf_nextd_off(hdr, struct udp_hdr *, hdrlen));
		break;
	case IPPROTO_ICMP:
		icmp_input(cur_fg,pkt,
			   mbuf_nextd_off(hdr, struct icmp_hdr *, hdrlen),
			   pktlen);
		break;
	}

	return;

out:
	mbuf_free(pkt);
}

/**
 * eth_input - process an ethernet packet
 * @pkt: the mbuf containing the packet
 */
void eth_input(struct eth_rx_queue *rx_queue, struct mbuf *pkt)
{
	struct eth_hdr *ethhdr = mbuf_mtod(pkt, struct eth_hdr *);
	struct eth_fg *fg;

	//set_current_queue(rx_queue);
	fg = fgs[pkt->fg_id];
	eth_fg_set_current(fg);

	log_debug("ip: got ethernet packet of len %ld, type %x\n",
		  pkt->len, ntoh16(ethhdr->type));

#ifdef ENABLE_PCAP
	pcap_write(pkt);
#endif

	if (ethhdr->type == hton16(ETHTYPE_IP))
		ip_input(fg, pkt, mbuf_nextd(ethhdr, struct ip_hdr *));
	else if (ethhdr->type == hton16(ETHTYPE_ARP))
		arp_input(pkt, mbuf_nextd(ethhdr, struct arp_hdr *));
	else
		mbuf_free(pkt);

//	unset_current_queue();
	unset_current_fg();
}

/* FIXME: change when we integrate better with LWIP */
/* NOTE: This function is only called for TCP */
int ip_output_hinted(struct eth_fg *cur_fg,struct pbuf *p, struct ip_addr *src, struct ip_addr *dest,
	      uint8_t ttl, uint8_t tos, uint8_t proto, uint8_t *dst_eth_addr)
{
	int ret;
	struct mbuf *pkt;
	struct eth_hdr *ethhdr;
	struct ip_hdr *iphdr;
	unsigned char *payload;
	struct pbuf *curp;
	struct ip_addr dst_addr;

	pkt = mbuf_alloc_local();
	if (unlikely(!pkt))
		return -ENOMEM;

	ethhdr = mbuf_mtod(pkt, struct eth_hdr *);
	iphdr = mbuf_nextd(ethhdr, struct ip_hdr *);
	payload = mbuf_nextd(iphdr, unsigned char *);

	if (dst_eth_addr && dst_eth_addr[0]) { 
		memcpy(&ethhdr->dhost,dst_eth_addr,6);
	} else {
		dst_addr.addr = ntoh32(dest->addr);
		if (arp_lookup_mac(&dst_addr, &ethhdr->dhost)) {
			log_err("ARP lookup failed.\n");
			mbuf_free(pkt);
			return -EIO;
		}
		if (dst_eth_addr) 
			memcpy(dst_eth_addr,&ethhdr->dhost,6);
	}

	ethhdr->shost = cfg_mac;
	ethhdr->type = hton16(ETHTYPE_IP);

	ip_setup_header(iphdr, proto, ntoh32(src->addr),
			ntoh32(dest->addr), p->tot_len);
	iphdr->tos = tos;
	iphdr->ttl = ttl;
	
	for (curp = p; curp; curp = curp->next) {
		memcpy(payload, curp->payload, curp->len);
		payload += curp->len;
	}

	/* Offload IP and TCP tx checksums */
	pkt->ol_flags = PKT_TX_IP_CKSUM;
	pkt->ol_flags |= PKT_TX_TCP_CKSUM;

	ret = eth_send_one(percpu_get(eth_txqs)[cur_fg->dev_idx],pkt, sizeof(struct eth_hdr) +
				sizeof(struct ip_hdr) + p->tot_len);
	if (unlikely(ret)) {
		mbuf_free(pkt);
		return -EIO;
	}

	return 0;
}

