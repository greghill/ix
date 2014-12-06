/*
 * ethqueue.c - ethernet queue support
 */

#include <ix/stddef.h>
#include <ix/kstats.h>
#include <ix/ethdev.h>
#include <ix/log.h>
#include <ix/control_plane.h>

/* Accumulate metrics period (in us) */
#define METRICS_PERIOD_US 10000

#define EMA_SMOOTH_FACTOR 0.01

DEFINE_PERCPU(int, eth_num_queues);
DEFINE_PERCPU(struct eth_rx_queue *, eth_rxqs[NETHDEV]);
DEFINE_PERCPU(struct eth_tx_queue *, eth_txqs[NETHDEV]);

struct metrics_accumulator {
	long timestamp;
	long queuing_delay;
	int batch_size;
	int count;
};

static DEFINE_PERCPU(struct metrics_accumulator, metrics_acc);

/* FIXME: convert to per-flowgroup */
//DEFINE_PERQUEUE(struct eth_tx_queue *, eth_txq);

unsigned int eth_rx_max_batch = 64;

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
	unsigned long min_timestamp = -1;
	unsigned long timestamp;
	int value;
	double val;
	struct metrics_accumulator *this_metrics_acc = &percpu_get(metrics_acc);

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
			struct mbuf *pos = rxq->head;
			if (pos)
				min_timestamp = min(min_timestamp, pos->timestamp);
			if (!eth_process_recv_queue(rxq)) {
				count++;
				empty = false;
			}
		}
	} while (!empty && count < eth_rx_max_batch);

	timestamp = rdtsc();
	this_metrics_acc->count++;
	value = count ? (timestamp - min_timestamp) / cycles_per_us : 0;
	this_metrics_acc->queuing_delay += value;
	this_metrics_acc->batch_size += count;
	if (timestamp - this_metrics_acc->timestamp > (long) cycles_per_us * METRICS_PERIOD_US) {
		if (this_metrics_acc->batch_size)
			val = (double) this_metrics_acc->queuing_delay / this_metrics_acc->batch_size;
		else
			val = 0;
		EMA_UPDATE(cp_shmem->cpu_metrics[percpu_get(cpu_nr)].queuing_delay, val, EMA_SMOOTH_FACTOR);
		if (this_metrics_acc->count)
			val = (double) this_metrics_acc->batch_size / this_metrics_acc->count;
		else
			val = 0;
		EMA_UPDATE(cp_shmem->cpu_metrics[percpu_get(cpu_nr)].batch_size, val, EMA_SMOOTH_FACTOR);
		this_metrics_acc->timestamp = timestamp;
		this_metrics_acc->count = 0;
		this_metrics_acc->queuing_delay = 0;
		this_metrics_acc->batch_size = 0;
	}

	KSTATS_PACKETS_INC(count);
	KSTATS_BATCH_INC(count);
#ifdef ENABLE_KSTATS
	int backlog = 0;
	for (i = 0; i < percpu_get(eth_num_queues); i++) {
		struct eth_rx_queue *rxq = percpu_get(eth_rxqs[i]);
		backlog += rxq->len;
	}
	backlog = div_up(backlog, eth_rx_max_batch);
	KSTATS_BACKLOG_INC(backlog);
#endif

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

