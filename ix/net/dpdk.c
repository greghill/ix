/*
 * dpdk.c - a network interface driver for DPDK
 */

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <rte_config.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>

#include <asm/cpu.h>

#include "ipv4.h"
#include "cfg.h"

#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define NB_MBUF   8192

#define DPDK_NUM_RX_DESC 128
#define DPDK_NUM_TX_DESC 512

/*
 * RX and TX Prefetch, Host, and Write-back threshold values should be
 * carefully set for optimal performance. Consult the network
 * controller's datasheet and supporting DPDK documentation for guidance
 * on how these parameters should be set.
 */
#define RX_PTHRESH 8 /**< Default values of RX prefetch threshold reg. */
#define RX_HTHRESH 8 /**< Default values of RX host threshold reg. */
#define RX_WTHRESH 4 /**< Default values of RX write-back threshold reg. */

/*
 * These default values are optimized for use with the Intel(R) 82599 10 GbE
 * Controller and the DPDK ixgbe PMD. Consider using other values for other
 * network controllers and/or network drivers.
 */
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */

static const struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 1, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
	},
	.txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
};

static const struct rte_eth_rxconf rx_conf = {
	.rx_thresh = {
		.pthresh = RX_PTHRESH,
		.hthresh = RX_HTHRESH,
		.wthresh = RX_WTHRESH,
	},
};

static const struct rte_eth_txconf tx_conf = {
	.tx_thresh = {
		.pthresh = TX_PTHRESH,
		.hthresh = TX_HTHRESH,
		.wthresh = TX_WTHRESH,
	},
	.tx_free_thresh = 0, /* Use PMD default values */
	.tx_rs_thresh = 0, /* Use PMD default values */
};

struct rte_mempool *dpdk_pool;
int dpdk_port;

#define MAX_PKT_BURST	32

static int dpdk_probe_one(int port)
{
	int ret;
	struct rte_eth_link link;
	struct ether_addr macaddr;
	
	ret = rte_eth_dev_configure(port, 1, 1, &port_conf);
	if (ret < 0)
		return ret;

	rte_eth_macaddr_get(port, &macaddr);
	memcpy((void *) &cfg_mac, macaddr.addr_bytes, ETHER_ADDR_LEN);
	
	ret = rte_eth_rx_queue_setup(port, 0, DPDK_NUM_RX_DESC,
				     rte_eth_dev_socket_id(port), &rx_conf,
				     dpdk_pool);
	if (ret < 0)
		return ret;
	
	ret = rte_eth_tx_queue_setup(port, 0, DPDK_NUM_TX_DESC,
				     rte_eth_dev_socket_id(port), &tx_conf);
	if (ret < 0)
		return ret;
	
	ret = rte_eth_dev_start(port);
	if (ret < 0)
		return ret;

	printf("Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
		(unsigned) port,
		macaddr.addr_bytes[0],
		macaddr.addr_bytes[1],
		macaddr.addr_bytes[2],
		macaddr.addr_bytes[3],
		macaddr.addr_bytes[4],
		macaddr.addr_bytes[5]);

	rte_eth_link_get(port, &link);
	if (!link.link_status)
		goto link_down;

	printf(" Link Up - speed %u Mbps - %s\n",
		(uint32_t) link.link_speed,
		(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
		("full-duplex") : ("half-duplex\n"));

	rte_eth_promiscuous_disable(port);
	rte_eth_allmulticast_enable(port);

	dpdk_port = port;

	return 0;

link_down:
	printf(" Link Down - moving on to next port\n");
	rte_eth_dev_stop(port);
	rte_eth_dev_close(port);
	return -EIO;
}

int dpdk_init(int argc, char *argv[])
{
	int nb_ports, i, ret;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

	dpdk_pool = rte_mempool_create("mbuf_pool", NB_MBUF,
			MBUF_SIZE, 32,
			sizeof(struct rte_pktmbuf_pool_private),
			rte_pktmbuf_pool_init, NULL,
			rte_pktmbuf_init, NULL,
			rte_socket_id(), 0);
	if (!dpdk_pool)
		return -ENOMEM;

	if (rte_pmd_init_all() < 0)
		return -EIO;
	if (rte_eal_pci_probe() < 0)
		return -EIO;

	nb_ports = rte_eth_dev_count();
	if (!nb_ports) {
		printf("unable to find any ports\n");
		return -ENODEV;
	}
	
	for (i = 0; i < nb_ports; i++) {
		ret = dpdk_probe_one(i);
		if (!ret) {
			printf("DPDK is ready\n");
			return 0;
		}
	}

	return -ENODEV;
}
