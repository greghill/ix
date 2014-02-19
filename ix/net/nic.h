#pragma once

#include <rte_config.h>
#include <rte_mbuf.h>

struct nic_operations {
	void (*init) (void);
	int (*has_pending_pkts) (void);
	int (*receive_one_pkt) (struct rte_mbuf **pkt);
	int (*tx_one_pkt) (struct rte_mbuf *pkt);
	struct rte_mbuf * (*alloc_pkt) (void);
	void (*free_pkt) (struct rte_mbuf *pkt);
};

extern struct nic_operations *nic_ops;
