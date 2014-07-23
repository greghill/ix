/*
 * ethqueue.c - ethernet queue support
 */

#include <ix/stddef.h>
#include <ix/kstats.h>
#include <ix/ethqueue.h>
#include <ix/queue.h>
#include <ix/log.h>

DEFINE_PERCPU(struct eth_rx_queue *, eth_rx);
DEFINE_PERCPU(struct eth_tx_queue *, eth_tx);

DEFINE_PERCPU(struct eth_recv_data, eth_rdata);
DEFINE_PERCPU(struct eth_send_data, eth_sdata);
DEFINE_PERCPU(struct mbuf *, eth_send_buf[ETH_DEV_TX_QUEUE_SZ]);

/**
 * eth_process_recv - processes packets pending to be received
 *
 * Returns true if there are no remaining packets.
 */
int eth_process_recv(void)
{
	struct eth_recv_data *rdata = &percpu_get(eth_rdata);
	struct mbuf *pos = rdata->head;
	int count = 0;

#ifdef ENABLE_KSTATS
	kstats_accumulate tmp;
#endif

	while (pos) {
		KSTATS_PUSH(eth_input, &tmp);
		eth_input(percpu_get(eth_rx), pos);
		KSTATS_POP(&tmp);

		pos = pos->next;
		if (++count > ETH_RX_MAX_BATCH)
			break;
	}

	rdata->head = pos;
	rdata->len -= count;

	return !pos;
}

/**
 * eth_process_send - processes packets pending to be sent
 */
void eth_process_send(void)
{
	struct eth_send_data *sdata = &percpu_get(eth_sdata);
	int nr;

	nr = eth_tx_xmit(percpu_get(eth_tx), sdata->len,
			 percpu_get(eth_send_buf));
	if (unlikely(nr != sdata->len))
		panic("transmit buffer size mismatch\n");

	sdata->len = 0;
}

