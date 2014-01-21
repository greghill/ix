/*
 * net.h - a global network stack local header
 */

#pragma once

#include <ix/types.h>

#include <net/arp.h>
#include <net/icmp.h>

#include <rte_config.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>

#define next_hdr_off(buf, type, off) \
	((type) ((uintptr_t) buf + off))
#define next_hdr(buf, type) \
	next_hdr_off(buf, type, sizeof(typeof(*buf)))
#define len_left(mbuf, ptr) \
	((uintptr_t) (ptr) - (uintptr_t) (mbuf)->pkt.data)
#define enough_space(mbuf, ptr)				\
	((uintptr_t) (ptr) + sizeof(typeof(*ptr)) -	\
	 (uintptr_t) (mbuf)->pkt.data <= rte_pktmbuf_data_len(mbuf))

extern struct rte_mempool *dpdk_pool;
extern int dpdk_port;

static inline int dpdk_tx_pkts(struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	return rte_eth_tx_burst(dpdk_port, 0, tx_pkts, nb_pkts);
}

static inline int dpdk_tx_one_pkt(struct rte_mbuf *pkt)
{
	struct rte_mbuf *pkts[1];
	pkts[0] = pkt;
	return dpdk_tx_pkts(pkts, 1);
}

static inline struct rte_mbuf *dpdk_alloc_mbuf(void)
{
	return rte_pktmbuf_alloc(dpdk_pool);
}

/* Address Resolution Protocol (ARP) definitions */
extern int arp_lookup_mac(struct ip_addr *addr, struct eth_addr *mac);
extern void arp_input(struct rte_mbuf *pkt, struct arp_hdr *hdr);
extern int arp_init(void);

/* Internet Control Message Protocol (ICMP) definitions */
extern void icmp_input(struct rte_mbuf *pkt, struct icmp_hdr *hdr, int len);

