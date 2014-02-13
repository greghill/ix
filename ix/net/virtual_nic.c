#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <ix/mempool.h>
#include <ix/queue.h>

#include "nic.h"

static struct rte_mbuf mypkt;

struct packet {
	int size;
	char data[];
};

static struct {
	struct queue rx_queue;
	struct queue tx_queue;
	char mempool_area[];
} *shmem;

static struct mempool mempool;

void *mempool_base;

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

static void *queue_item_alloc(void)
{
	void *ret = (void *) (mempool_alloc(&mempool) - mempool_base);
	return ret;
}

static void virtual_init(void)
{
	int shmfd;

	shmfd = shm_open("/vnic_shmem", O_RDWR | O_CREAT, 0777);
	ftruncate(shmfd, 4*1<<20);
	shmem = mmap(NULL, 4*1<<20, PROT_READ|PROT_WRITE, MAP_SHARED, shmfd, 0);

	mempool_base = shmem->mempool_area;

	prealloc_mempool_create(mempool_base, &mempool, 64, 4096);

	/* HACK: Allocate and throw away first element (acts as NULL). */
	mempool_alloc(&mempool);

	queue_init(&shmem->rx_queue, queue_item_alloc);
	queue_init(&shmem->tx_queue, queue_item_alloc);

	queue_populate(&shmem->rx_queue);
}

static int virtual_has_pending_pkts(void)
{
	return !queue_empty(&shmem->rx_queue);
}

static int virtual_receive_one_pkt(struct rte_mbuf **pkt)
{
	struct packet *packet;

	packet = (struct packet *) ((unsigned long long) queue_dequeue(&shmem->rx_queue) + mempool_base);

	if (packet) {
		pkt[0] = &mypkt;
		mypkt.pkt.data = packet->data;
		mypkt.pkt.data_len = packet->size;
		/* HACK: Store packet address. */
		mypkt.pkt.next = (void*) packet - mempool_base;
		return 1;
	}

	return 0;
}

static int virtual_tx_one_pkt(struct rte_mbuf *pkt)
{
	int i;
	struct packet *packet;

	for (i = 0; i < QUEUE_SIZE; i++) {
		if (!shmem->tx_queue.item[i].done || !shmem->tx_queue.item[i].data_offset)
			continue;
		mempool_free(&mempool, shmem->tx_queue.item[i].data_offset + mempool_base);
		shmem->tx_queue.item[i].data_offset = 0;
	}

	/* HACK: Until we get rid of rte_mbuf. */
	packet = (struct packet *) ((void *) pkt[0].pkt.next + (unsigned long long) mempool_base);
	packet->size = pkt[0].pkt.data_len;
	if (packet->data != pkt[0].pkt.data) {
		fprintf(stderr, "packet not reused %p %p\n", packet->data, pkt[0].pkt.data);
		exit(1);
	}

	return queue_enqueue(&shmem->tx_queue, (void *) ((void *) packet - mempool_base));
}

static void virtual_free_pkt(struct rte_mbuf *pkt)
{
}

static struct rte_mbuf *virtual_alloc_pkt()
{
	return 0;
}

struct nic_operations virtual_nic_ops = {
  .init             = virtual_init,
  .has_pending_pkts = virtual_has_pending_pkts,
  .receive_one_pkt  = virtual_receive_one_pkt,
  .tx_one_pkt       = virtual_tx_one_pkt,
  .free_pkt         = virtual_free_pkt,
  .alloc_pkt        = virtual_alloc_pkt,
};
