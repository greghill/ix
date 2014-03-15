#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <ix/errno.h>
#include <ix/ethdev.h>
#include <ix/log.h>
#include <ix/mempool.h>

#define QUEUE_SIZE 16

struct descriptor {
	unsigned int done;
	unsigned int len;
	unsigned long long addr;
};

struct queue_entry {
        struct mbuf *mbuf;
};

struct rx_queue {
	unsigned int pos;
	unsigned int len;
	struct descriptor *ring;
	struct queue_entry ring_entries[QUEUE_SIZE];
} rx_queue;

struct tx_queue {
	unsigned int head;
	unsigned int tail;
	unsigned int len;
	struct descriptor *ring;
	struct queue_entry ring_entries[QUEUE_SIZE];
} tx_queue;

static struct {
	unsigned int rx_head;
	unsigned int tx_head;
	unsigned int rx_tail;
	unsigned int tx_tail;
	struct descriptor rx_ring[QUEUE_SIZE];
	struct descriptor tx_ring[QUEUE_SIZE];
	struct mbuf mempool_area[];
} *shmem;

static struct mempool mempool;

void *mempool_base;

struct eth_addr virtual_eth_addr = { .addr = {0x00, 0x16, 0x3e, 0x1f, 0x4a, 0x6b} };
static struct rte_eth_dev_data virtual_eth_dev_data;
static struct rte_eth_dev virtual_eth_dev;

static struct mbuf *virtual_mbuf_alloc(struct mempool *pool)
{
        struct mbuf *m = mempool_alloc(pool);
        if (unlikely(!m))
                return NULL;

        m->pool = pool;
        m->next = NULL;

        return m;
}

static inline machaddr_t virtual_mbuf_machaddr(struct mbuf *m)
{
	return mbuf_mtod(m, void *) - mempool_base;
}

static int virtual_poll(struct eth_rx_queue *rx)
{
	struct rx_queue *rxq = &rx_queue;
	volatile struct descriptor *rxdp;
	struct mbuf *b, *new_b;
	struct queue_entry *rxqe;
	int nb_descs = 0;

	while (1) {
		rxdp = &rxq->ring[rxq->pos & (rxq->len - 1)];
		if (!rxdp->done)
			break;

		rxqe = &rxq->ring_entries[rxq->pos & (rxq->len -1)];
		b = rxqe->mbuf;
		b->len = rxdp->len;

		new_b = virtual_mbuf_alloc(&mempool);
		if (unlikely(!new_b)) {
			log_err("ixgbe: unable to allocate RX mbuf\n");
			return nb_descs;
		}

		rxqe->mbuf = new_b;
		rxdp->addr = virtual_mbuf_machaddr(new_b);
		rxdp->done = 0;

		eth_input(b);

		rxq->pos++;
		nb_descs++;
	}

	if (nb_descs) {
		/* inform HW that more descriptors have become available */
		shmem->rx_tail = (rxq->pos - 1) & (rxq->len - 1);
	}

	return nb_descs;
}

static int virtual_reclaim(struct eth_tx_queue *tx)
{
	struct tx_queue *txq = &tx_queue;
	struct queue_entry *txe;
	struct descriptor *txdp;
	int idx = 0, nb_desc = 0;

	while ((uint16_t) (txq->head + idx) != txq->tail) {
		txe = &txq->ring_entries[(txq->head + idx) & (txq->len - 1)];

		if (!txe->mbuf) {
			idx++;
			continue;
		}

		txdp = &txq->ring[(txq->head + idx) & (txq->len - 1)];
		if (!txdp->done)
			break;

		mbuf_xmit_done(txe->mbuf);
		txe->mbuf = NULL;
		idx++;
		nb_desc = idx;
	}

	txq->head += nb_desc;
	return (uint16_t) (txq->len + txq->head - txq->tail);
}

static int virtual_tx_xmit_one(struct tx_queue *txq, struct mbuf *mbuf)
{
	struct descriptor *txdp;

	if (unlikely((uint16_t) (txq->tail - txq->head) >= txq->len))
		return -EAGAIN;

	txq->ring_entries[(txq->tail) & (txq->len - 1)].mbuf = mbuf;

	txdp = &txq->ring[txq->tail & (txq->len - 1)];
	txdp->addr = virtual_mbuf_machaddr(mbuf);
	txdp->len = mbuf->len;
	txdp->done = 0;

	txq->tail++;

	return 0;
}

static int virtual_xmit(struct eth_tx_queue *tx, int nr, struct mbuf **mbufs)
{
	struct tx_queue *txq = &tx_queue;
	int nb_pkts = 0;

	while (nb_pkts < nr) {
		if (virtual_tx_xmit_one(txq, mbufs[nb_pkts]))
			break;

		nb_pkts++;
	}

	shmem->tx_tail = txq->tail & (txq->len - 1);

	return nb_pkts;
}

static struct eth_rx_queue virtual_eth_rx = {
	.poll    = virtual_poll,
};

static struct eth_tx_queue virtual_eth_tx = {
	.reclaim = virtual_reclaim,
	.xmit    = virtual_xmit,
};

/* Verbatim copy from ix/core/mempool.c. */
static void mempool_init_buf(struct mempool *m, int nr_elems, size_t elem_len)
{
	int i;
	uintptr_t pos = (uintptr_t) m->buf;
	uintptr_t next_pos = pos + elem_len;

	m->head = (struct mempool_hdr *)pos;

	for (i = 0; i < nr_elems - 1; i++) {
		((struct mempool_hdr *)pos)->next =
			(struct mempool_hdr *)next_pos;

		pos = next_pos;
		next_pos += elem_len;
	}

	((struct mempool_hdr *)pos)->next = NULL;
}

/* Temporary hack to create the mempool inside the shared memory region. Copied from ix/core/mempool.c. */
static int prealloc_mempool_create(void *buf, struct mempool *m, int nr_elems, size_t elem_len)
{
	int nr_pages;

	if (!elem_len || !nr_elems)
		return -EINVAL;

	elem_len = align_up(elem_len, sizeof(long));
	nr_pages = PGN_2MB(nr_elems * elem_len + PGMASK_2MB);
	nr_elems = nr_pages * PGSIZE_2MB / elem_len;
	m->nr_pages = nr_pages;

	m->buf = buf;

	mempool_init_buf(m, nr_elems, elem_len);
	return 0;
}

int virtual_init(void)
{
	int shmfd;
	int i;
	struct mbuf *new_b;

	shmfd = shm_open("/vnic_shmem", O_RDWR | O_CREAT, 0777);
	if (ftruncate(shmfd, 4*1<<20)) {
		return -EINVAL;
	}
	shmem = mmap(NULL, 4*1<<20, PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);

	mempool_base = shmem->mempool_area;

	prealloc_mempool_create(mempool_base, &mempool, 64, 4096);

	/* HACK: Allocate and throw away first element (acts as NULL). */
	mempool_alloc(&mempool);

	tx_queue.len = QUEUE_SIZE;
	tx_queue.head = 0;
	tx_queue.tail = 0;
	tx_queue.ring = shmem->tx_ring;
	shmem->tx_head = 0;
	shmem->tx_tail = 0;

	rx_queue.len = QUEUE_SIZE;
	rx_queue.ring = shmem->rx_ring;
	shmem->rx_head = 0;
	shmem->rx_tail = QUEUE_SIZE - 1;

	for (i = 0; i < rx_queue.len; i++) {
		new_b = virtual_mbuf_alloc(&mempool);
		if (unlikely(!new_b)) {
			log_err("ixgbe: unable to allocate RX mbuf\n");
			return -ENOMEM;
		}

		rx_queue.ring_entries[i].mbuf = new_b;
		rx_queue.ring[i].addr = virtual_mbuf_machaddr(new_b);
		rx_queue.ring[i].done = 0;
	}

	virtual_eth_dev.data = &virtual_eth_dev_data;
	virtual_eth_dev_data.mac_addrs = &virtual_eth_addr;

	eth_dev = &virtual_eth_dev;
	eth_rx = &virtual_eth_rx;
	eth_tx = &virtual_eth_tx;

	return 0;
}
