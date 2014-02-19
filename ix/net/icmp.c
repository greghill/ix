/*
 * icmp.c - Internet Control Message Protocol support
 *
 * See RFC 792 for more details.
 */

#define DEBUG 1

#include <ix/stddef.h>
#include <ix/errno.h>
#include <ix/log.h>

#include <asm/chksum.h>

#include <net/ethernet.h>
#include <net/ip.h>
#include <net/icmp.h>

#include "net.h"
#include "cfg.h"

static int icmp_reflect(struct mbuf *pkt, struct icmp_hdr *hdr, int len)
{
	struct eth_hdr *ethhdr = mbuf_mtod(pkt, struct eth_hdr *);
	struct ip_hdr *iphdr = mbuf_nextd(ethhdr, struct ip_hdr *);
	int ret;

	ethhdr->dhost = ethhdr->shost;
	ethhdr->shost = cfg_mac;

	/* FIXME: check for invalid (e.g. multicast) src addr */
	iphdr->dst_addr = iphdr->src_addr;
	iphdr->src_addr.addr = hton32(cfg_host_addr.addr);

	hdr->chksum = 0;
	hdr->chksum = chksum_internet((void *) hdr, len);

	ret = eth_tx_xmit_one(eth_tx, pkt, pkt->len);

	if (unlikely(ret != 1)) {
		mbuf_free(pkt);
		return -EIO;
	}

	return 0;
}

/*
 * icmp_input - handles an input ICMP packet
 * @pkt: the packet
 * @hdr: the ICMP header
 */
void icmp_input(struct mbuf *pkt, struct icmp_hdr *hdr, int len)
{
	if (len < ICMP_MINLEN)
		goto out;
	if (chksum_internet((void *) hdr, len))
		goto out;

	log_debug("icmp: got request type %d, code %d\n",
		  hdr->type, hdr->code);

	switch(hdr->type) {
	case ICMP_ECHO:
		hdr->type = ICMP_ECHOREPLY;
		icmp_reflect(pkt, hdr, len);
		break;
	default:
		goto out;
	}

	return;

out:
	mbuf_free(pkt);
}

