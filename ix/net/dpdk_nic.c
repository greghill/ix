#include <rte_config.h>

#include "net.h"
#include "nic.h"

static int dpdk_has_pending_pkts(void)
{
	return rte_eth_rx_queue_count(dpdk_port, 0) > 0;
}

static int dpdk_receive_one_pkt(struct rte_mbuf **pkt)
{
	return rte_eth_rx_burst(dpdk_port, 0, pkt, 1);
}

static void dpdk_free_pkt(struct rte_mbuf *pkt)
{
	rte_pktmbuf_free(pkt);
}

static struct rte_mbuf *dpdk_alloc_pkt()
{
	return dpdk_alloc_mbuf();
}

struct nic_operations dpdk_nic_ops = {
  .has_pending_pkts = dpdk_has_pending_pkts,
  .receive_one_pkt  = dpdk_receive_one_pkt,
  .tx_one_pkt       = dpdk_tx_one_pkt,
  .free_pkt         = dpdk_free_pkt,
  .alloc_pkt        = dpdk_alloc_pkt,
};
