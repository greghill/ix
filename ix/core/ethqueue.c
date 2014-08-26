/*
 * ethqueue.c - ethernet queue support
 */

#include <ix/stddef.h>
#include <ix/kstats.h>
#include <ix/ethdev.h>
#include <ix/queue.h>
#include <ix/log.h>

DEFINE_PERCPU(int, eth_num_queues);
DEFINE_PERCPU(struct eth_rx_queue *, eth_rxqs[NETHDEV]);
DEFINE_PERCPU(struct eth_tx_queue *, eth_txqs[NETHDEV]);

/* FIXME: convert to per-flowgroup */
DEFINE_PERQUEUE(struct eth_tx_queue *, eth_txq);


/**
 * eth_process_poll - polls HW for new packets
 *
 * Returns the number of new packets received.
 */
int eth_process_poll(void)
{
	int i, count = 0;
	struct eth_rx_queue *rxq;

	for (i = 0; i < percpu_get(eth_num_queues); i++) {
		rxq = percpu_get(eth_rxqs[i]);
		count += eth_rx_poll(rxq);
	}

	return count;
}

static int eth_process_recv_queue(struct eth_rx_queue *rxq)
{
	struct mbuf *pos = rxq->head;
#ifdef ENABLE_KSTATS
	kstats_accumulate tmp;
#endif

	if (!pos)
		return -EAGAIN;

	/* NOTE: pos could get freed after eth_input(), so check next here */
	rxq->head = pos->next;
	rxq->len--;

	KSTATS_PUSH(eth_input, &tmp);
	eth_input(rxq, pos);
	KSTATS_POP(&tmp);

	return 0;
}

/**
 * eth_process_recv - processes pending received packets
 *
 * Returns true if there are no remaining packets.
 */
int eth_process_recv(void)
{
	int i, count = 0;
	bool empty;

	/*
	 * We round robin through each queue one packet at
	 * a time for fairness, and stop when all queues are
	 * empty or the batch limit is hit. We're okay with
	 * going a little over the batch limit if it means
	 * we're not favoring one queue over another.
	 */
	do {
		empty = true;
		for (i = 0; i < percpu_get(eth_num_queues); i++) {
			struct eth_rx_queue *rxq = percpu_get(eth_rxqs[i]);
			if (!eth_process_recv_queue(rxq)) {
				count++;
				empty = false;
			}
		}
	} while (!empty && count < ETH_RX_MAX_BATCH);

	KSTATS_PACKETS_INC(count);
	KSTATS_BATCH_INC(count);

	return empty;
}

/**
 * eth_process_send - processes packets pending to be sent
 */
void eth_process_send(void)
{
	int i, nr;
	struct eth_tx_queue *txq;

	for (i = 0; i < percpu_get(eth_num_queues); i++) {
		txq = percpu_get(eth_txqs[i]);

		nr = eth_tx_xmit(txq, txq->len, txq->bufs);
		if (unlikely(nr != txq->len))
			panic("transmit buffer size mismatch\n");

		txq->len = 0;
	}
}

/**
 * eth_process_reclaim - processs packets that have completed sending
 */
void eth_process_reclaim(void)
{
	int i;
	struct eth_tx_queue *txq;

	for (i = 0; i < percpu_get(eth_num_queues); i++) {
		txq = percpu_get(eth_txqs[i]);
		txq->cap = eth_tx_reclaim(txq);
	}
}

