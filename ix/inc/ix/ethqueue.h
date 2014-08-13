/*
 * ethqueue.h - ethernet queue support
 */

#pragma once

#include <ix/cpu.h>
#include <ix/mbuf.h>
#include <ix/errno.h>
#include <ix/bitmap.h>
#include <ix/ethfg.h>

#define ETH_DEV_RX_QUEUE_SZ     512
#define ETH_DEV_TX_QUEUE_SZ     1024
#define ETH_RX_MAX_DEPTH	1024
#define ETH_RX_MAX_BATCH	16


/*
 * Recieve Queue API
 */

struct eth_rx_queue {
	int (*poll) (struct eth_rx_queue *rx);
	void *perqueue_offset;
	int queue_idx;
	DEFINE_BITMAP(assigned_fgs, ETH_MAX_NUM_FG);
};

DECLARE_PERCPU(struct eth_rx_queue *, eth_rx);

/**
 * eth_rx_poll - recieve pending packets on an RX queue
 * @rx: the RX queue
 *
 * Returns the number of packets recieved.
 */
static inline int eth_rx_poll(struct eth_rx_queue *rx)
{
	return rx->poll(rx);
}

struct eth_recv_data {
	struct mbuf *head, *tail;
	int len;
};

DECLARE_PERCPU(struct eth_recv_data, eth_rdata);

/**
 * eth_recv - enqueues a received packet
 * @mbuf: the packet
 *
 * Typically called by the device driver's poll() routine.
 *
 * Returns 0 if successful, otherwise the packet should be dropped.
 */
static inline int eth_recv(struct mbuf *mbuf)
{
	struct eth_recv_data *rdata = &percpu_get(eth_rdata);

	if (unlikely(rdata->len >= ETH_RX_MAX_DEPTH))
		return -EBUSY;

	mbuf->next = NULL;

	if (!rdata->head) {
		rdata->head = mbuf;
		rdata->tail = mbuf;
	} else {
		rdata->tail->next = mbuf;
		rdata->tail = mbuf;
	}

	rdata->len++;
	return 0;
}

/**
 * eth_process_poll - polls for new packets
 *
 * Returns the number of new packets available.
 */
static inline int eth_process_poll(void)
{
	return eth_rx_poll(percpu_get(eth_rx));
}

extern int eth_process_recv(void);
extern bool eth_rx_idle_wait(uint64_t max_usecs);


/*
 * Transmit Queue API
 */

struct eth_tx_queue {
	int (*reclaim) (struct eth_tx_queue *tx);
	int (*xmit) (struct eth_tx_queue *tx, int nr, struct mbuf **mbufs);
};

DECLARE_PERCPU(struct eth_tx_queue *, eth_tx);

/**
 * eth_tx_reclaim - scans the queue and reclaims finished buffers
 * @tx: the TX queue
 *
 * NOTE: scatter-gather mbuf's can span multiple descriptors, so
 * take that into account when interpreting the count provided by
 * this function.
 *
 * Returns an available descriptor count.
 */
static inline int eth_tx_reclaim(struct eth_tx_queue *tx)
{
	return tx->reclaim(tx);
}

/**
 * eth_tx_xmit - transmits packets on a TX queue
 * @tx: the TX queue
 * @nr: the number of mbufs to transmit
 * @mbufs: an array of mbufs to process
 *
 * Returns the number of mbuf's transmitted.
 */
static inline int eth_tx_xmit(struct eth_tx_queue *tx,
			      int nr, struct mbuf **mbufs)
{
#ifdef ENABLE_PCAP
	int i;

	for (i = 0; i < nr; i++)
		pcap_write(mbufs[i]);
#endif
	return tx->xmit(tx, nr, mbufs);
}

struct eth_send_data {
	int cap, len;
};

DECLARE_PERCPU(struct eth_send_data, eth_sdata);
DECLARE_PERCPU(struct mbuf *, eth_send_buf[ETH_DEV_TX_QUEUE_SZ]);

/**
 * eth_send - enqueues a packet to be sent
 * @mbuf: the packet
 *
 * Returns 0 if successful, otherwise out of space.
 */
static inline int eth_send(struct mbuf *mbuf)
{
	struct eth_send_data *sdata = &percpu_get(eth_sdata);
	int nr = 1 + mbuf->nr_iov;

	if (unlikely(nr > sdata->cap))
		return -EBUSY;

	percpu_get(eth_send_buf[sdata->len++]) = mbuf;
	sdata->cap -= nr;

	return 0;
}

/**
 * eth_send_one - enqueues a packet without scatter-gather to be sent
 * @mbuf: the packet
 * @len: the length of the packet
 *
 * Returns 0 if successful, otherwise out of space.
 */
static inline int eth_send_one(struct mbuf *mbuf, size_t len)
{
	mbuf->len = len;
	mbuf->nr_iov = 0;

	return eth_send(mbuf);
}

/**
 * eth_process_reclaim - processes completed sends
 */
static inline void eth_process_reclaim(void)
{
	percpu_get(eth_sdata).cap = eth_tx_reclaim(percpu_get(eth_tx));
}

extern void eth_process_send(void);

