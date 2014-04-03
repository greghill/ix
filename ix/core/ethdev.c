/*
 * ethdev.c - ethernet device support
 */

#include <string.h>

#include <ix/stddef.h>
#include <ix/errno.h>
#include <ix/ethdev.h>
#include <ix/log.h>
#include <ix/cpu.h>
#include <ix/queue.h>

#include <net/ethernet.h>

struct rte_eth_dev *eth_dev;
uint16_t eth_rx_count;
struct eth_rx_queue **eth_rx;
DEFINE_PERCPU(struct eth_tx_queue *, eth_tx);

DEFINE_PERCPU(int, tx_batch_cap);
DEFINE_PERCPU(int, tx_batch_len);
DEFINE_PERCPU(struct mbuf *, tx_batch[ETH_DEV_TX_QUEUE_SZ]);

static const struct rte_eth_conf default_conf = {
        .rxmode = {
                .split_hdr_size = 0,
                .header_split   = 0, /**< Header Split disabled */
                .hw_ip_checksum = 0, /**< IP checksum offload disabled */
                .hw_vlan_filter = 0, /**< VLAN filtering disabled */
                .jumbo_frame    = 0, /**< Jumbo Frame Support disabled */
                .hw_strip_crc   = 1, /**< CRC stripped by hardware */
                .mq_mode        = ETH_MQ_RX_RSS,
        },
	.rx_adv_conf = {
		.rss_conf = {
			.rss_hf = ETH_RSS_IPV4_TCP,
		},
	},
        .txmode = {
                .mq_mode = ETH_MQ_TX_NONE,
        },
};

/**
 * eth_dev_get_hw_mac - retreives the default MAC address
 * @dev: the ethernet device
 * @mac_addr: pointer to store the mac
 */
void eth_dev_get_hw_mac(struct rte_eth_dev *dev, struct eth_addr *mac_addr)
{
	memcpy(&mac_addr->addr[0], &dev->data->mac_addrs[0], ETH_ADDR_LEN);
}

/**
 * eth_dev_start - starts an ethernet device
 * @dev: the ethernet device
 * @tx_queues: number of requested tx queues
 *
 * Returns 0 if successful, otherwise failure.
 */
int eth_dev_start(struct rte_eth_dev *dev, unsigned int tx_queues)
{
	unsigned int rx_queue_idx;
	unsigned int tx_queue_idx;
	int ret;
	int i;
	struct eth_addr macaddr;
	struct rte_eth_link link;
	struct rte_eth_dev_info dev_info;

	if (tx_queues == 0)
		return -EINVAL;

	dev->dev_ops->dev_infos_get(dev, &dev_info);

	dev->data->nb_rx_queues = min(dev_info.max_rx_queues, ETH_RSS_RETA_MAX_QUEUE);
	dev->data->nb_tx_queues = tx_queues;

	dev->data->rx_queues = malloc(sizeof(struct eth_rx_queue *) * dev->data->nb_rx_queues);
	if (!dev->data->rx_queues)
		return -ENOMEM;

	dev->data->tx_queues = malloc(sizeof(struct eth_tx_queue *) * dev->data->nb_tx_queues);
	if (!dev->data->tx_queues) {
		ret = -ENOMEM;
		goto err;
	}

	for (rx_queue_idx = 0; rx_queue_idx < dev->data->nb_rx_queues; rx_queue_idx++) {
		ret = dev->dev_ops->rx_queue_setup(dev, rx_queue_idx, -1, ETH_DEV_RX_QUEUE_SZ);
		if (ret)
			goto err_rxsetup;
	}

	for (tx_queue_idx = 0; tx_queue_idx < dev->data->nb_tx_queues; tx_queue_idx++) {
		ret = dev->dev_ops->tx_queue_setup(dev, tx_queue_idx, -1, ETH_DEV_TX_QUEUE_SZ);
		if (ret)
			goto err_txsetup;
	}

	ret = dev->dev_ops->dev_start(dev);
	if (ret)
		goto err_start;

	dev->dev_ops->promiscuous_disable(dev);
	dev->dev_ops->allmulticast_enable(dev);

	eth_dev_get_hw_mac(dev, &macaddr);
	log_info("eth: started an ethernet device\n");
	log_info("eth:\tMAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
		 macaddr.addr[0], macaddr.addr[1],
		 macaddr.addr[2], macaddr.addr[3],
		 macaddr.addr[4], macaddr.addr[5]);

	dev->dev_ops->link_update(dev, 1);
	link = dev->data->dev_link;

	if (!link.link_status) {
		log_warn("eth:\tlink appears to be down, check connection.\n");
	} else {
		log_info("eth:\tlink up - speed %u Mbps, %s\n",
			 (uint32_t) link.link_speed,
			 (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
			 ("full-duplex") : ("half-duplex\n"));
	}

	eth_dev = dev;
	eth_rx_count = dev->data->nb_rx_queues;
	eth_rx = dev->data->rx_queues;
	percpu_get(eth_tx) = dev->data->tx_queues[0];

	percpu_get(assigned_queues) = (1 << eth_rx_count) - 1;

	for (i = 0; i < eth_rx_count; i++) {
		ret = queue_init_one(i);
		if (ret)
			goto err_start;
	}

	return 0;


err_start:
	dev->dev_ops->tx_queue_release(dev->data->tx_queues[0]);
err_txsetup:
	while (tx_queue_idx--)
		dev->dev_ops->tx_queue_release(dev->data->tx_queues[tx_queue_idx]);
err_rxsetup:
	while (rx_queue_idx--)
		dev->dev_ops->rx_queue_release(dev->data->rx_queues[rx_queue_idx]);
	free(dev->data->tx_queues);
err:
	free(dev->data->rx_queues);
	return ret;
}

/**
 * eth_dev_stop - stops an ethernet device
 * @dev: the ethernet device
 */
void eth_dev_stop(struct rte_eth_dev *dev)
{
	dev->dev_ops->dev_stop(dev);
	dev->dev_ops->tx_queue_release(dev->data->tx_queues[0]);
	dev->dev_ops->rx_queue_release(dev->data->rx_queues[0]);
	dev->data->nb_rx_queues = 0;
	dev->data->nb_tx_queues = 0;
	free(dev->data->tx_queues);
	free(dev->data->rx_queues);

	eth_dev = NULL;
	eth_rx = NULL;
	eth_tx = NULL;
} 

/**
 * eth_dev_alloc - allocates an ethernet device
 * @private_len: the size of the private area
 *
 * Returns an ethernet device, or NULL if failure.
 */
struct rte_eth_dev *eth_dev_alloc(size_t private_len)
{
	struct rte_eth_dev *dev;

	dev = malloc(sizeof(struct rte_eth_dev));
	if (!dev)
		return NULL;

	dev->pci_dev = NULL;
	dev->dev_ops = NULL;

	dev->data = malloc(sizeof(struct rte_eth_dev_data));
	if (!dev->data) {
		free(dev);
		return NULL;
	}

	memset(dev->data, 0, sizeof(struct rte_eth_dev_data));
	dev->data->dev_conf = default_conf;

	dev->data->dev_private = malloc(private_len);
	if (!dev->data->dev_private) {
		free(dev->data);
		free(dev);
		return NULL;
	}

	memset(dev->data->dev_private, 0, private_len);

	return dev;
}

/**
 * eth_dev_destroy - frees an ethernet device
 * @dev: the ethernet device
 */
void eth_dev_destroy(struct rte_eth_dev *dev)
{
	if (dev->dev_ops && dev->dev_ops->dev_close)
		dev->dev_ops->dev_close(dev);

	free(dev->data->dev_private);
	free(dev->data);
	free(dev);
}

