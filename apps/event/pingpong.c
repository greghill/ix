#include <ix/stddef.h>
#include <mempool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "ixev.h"

struct pp_conn {
	struct ixev_ctx ctx;
	size_t bytes_left;
	char data[];
};

static size_t msg_size;

static unsigned int pp_conn_pool_entries;
static __thread struct mempool pp_conn_pool;
static __thread int pp_conn_pool_count;

static void pp_main_handler(struct ixev_ctx *ctx, unsigned int reason);

static void pp_stream_handler(struct ixev_ctx *ctx, unsigned int reason)
{
	struct pp_conn *conn = container_of(ctx, struct pp_conn, ctx);
	size_t bytes_so_far = msg_size - conn->bytes_left;
	ssize_t ret;

	ret = ixev_send(ctx, &conn->data[bytes_so_far], conn->bytes_left);
	if (ret < 0) {
		if (ret != -EAGAIN)
			ixev_close(ctx);
		return;
	}

	conn->bytes_left -= ret;
	if (!conn->bytes_left) {
		conn->bytes_left = msg_size;
		ixev_set_handler(ctx, IXEVIN, &pp_main_handler);
	}
}

static void pp_main_handler(struct ixev_ctx *ctx, unsigned int reason)
{
	struct pp_conn *conn = container_of(ctx, struct pp_conn, ctx);
	ssize_t ret;

	while (1) {
		size_t bytes_so_far = msg_size - conn->bytes_left;

		ret = ixev_recv(ctx, &conn->data[bytes_so_far],
				conn->bytes_left);
		if (ret <= 0) {
			if (ret != -EAGAIN)
				ixev_close(ctx);
			return;
		}

		conn->bytes_left -= ret;
		if (conn->bytes_left)
			return;

		conn->bytes_left = msg_size;
		ret = ixev_send(ctx, &conn->data[0], conn->bytes_left);
		if (ret == -EAGAIN)
			ret = 0;
		if (ret < 0) {
			ixev_close(ctx);
			return;
		}

		conn->bytes_left -= ret;
		if (conn->bytes_left) {
			ixev_set_handler(ctx, IXEVOUT, &pp_stream_handler);
			return;
		}

		conn->bytes_left = msg_size;
	}
}

static struct ixev_ctx *pp_accept(struct ip_tuple *id)
{
	/* NOTE: we accept everything right now, did we want a port? */
	if (pp_conn_pool_count >= pp_conn_pool_entries) {
		fprintf(stderr, "Error: mempool ran out of space.\n");
		exit(1);
	}
	struct pp_conn *conn = mempool_alloc(&pp_conn_pool);
	if (!conn)
		return NULL;
	pp_conn_pool_count++;

	conn->bytes_left = msg_size;
	ixev_ctx_init(&conn->ctx);
	ixev_set_handler(&conn->ctx, IXEVIN, &pp_main_handler);

	return &conn->ctx;
}

static void pp_release(struct ixev_ctx *ctx)
{
	struct pp_conn *conn = container_of(ctx, struct pp_conn, ctx);

	mempool_free(&pp_conn_pool, conn);
	pp_conn_pool_count--;
}

struct ixev_conn_ops pp_conn_ops = {
	.accept		= &pp_accept,
	.release	= &pp_release,
};

static void *pp_main(void *arg)
{
	int ret;

	ret = ixev_init_thread();
	if (ret) {
		printf("unable to init IXEV\n");
		return NULL;
	};

	ret = mempool_create(&pp_conn_pool, pp_conn_pool_entries,
			     sizeof(struct pp_conn) + msg_size);
	if (ret) {
		printf("unable to create mempool\n");
		return NULL;
	}

	while (1) {
		ixev_wait();
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int i, nr_cpu;
	pthread_t tid;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s MSG_SIZE [MAX_CONNECTIONS]\n", argv[0]);
		return -1;
	}

	msg_size = atol(argv[1]);

	if (argc >= 3)
		pp_conn_pool_entries = atoi(argv[2]);
	else
		pp_conn_pool_entries = 4096;

	ixev_init(&pp_conn_ops);

	nr_cpu = sys_nrcpus();
	if (nr_cpu < 1) {
		printf("got invalid cpu count %d\n", nr_cpu);
		exit(-1);
	}
	nr_cpu--; /* don't count the main thread */

	sys_spawnmode(true);

	for (i = 0; i < nr_cpu; i++) {
		if (pthread_create(&tid, NULL, pp_main, NULL)) {
			printf("failed to spawn thread %d\n", i);
			exit(-1);
		}
	}

	pp_main(NULL);
	printf("exited\n");
	return 0;
}

