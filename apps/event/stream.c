#include <ix/stddef.h>
#include <mempool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "ixev.h"

#define BUF_SIZE	65536

#define ROUND_UP(num, multiple) ((((num) + (multiple) - 1) / (multiple)) * (multiple));

enum {
	STREAM_MODE_RECV,
	STREAM_MODE_SEND,
};

struct stream_conn {
	struct ixev_ctx ctx;
	int mode;
	size_t bytes_recvd;
	size_t bytes_sent;
	char data[BUF_SIZE];
};

static struct mempool_datastore stream_conn_datastore;
static __thread struct mempool stream_conn_pool;

static void stream_handler(struct ixev_ctx *ctx, unsigned int reason)
{
	struct stream_conn *conn = container_of(ctx, struct stream_conn, ctx);
	ssize_t ret;

	while (1) {
		if (conn->mode == STREAM_MODE_RECV) {
			ret = ixev_recv(ctx, conn->data, BUF_SIZE);
			if (ret <= 0) {
				if (ret != -EAGAIN)
					ixev_close(ctx);
				return;
			}
			conn->bytes_recvd = ret;
			conn->bytes_sent = 0;
			conn->mode = STREAM_MODE_SEND;
		} else {
			ret = ixev_send(ctx, &conn->data[conn->bytes_sent],
				        conn->bytes_recvd);
			if (ret == -EAGAIN) {
				ixev_set_handler(ctx, IXEVOUT, &stream_handler);
				return;
			}
			if (ret < 0)
				return;

			conn->bytes_recvd -= ret;
			conn->bytes_sent += ret;

			if (conn->bytes_recvd) {
				ixev_set_handler(ctx, IXEVOUT, &stream_handler);
				return;
			}

			ixev_set_handler(ctx, IXEVIN, &stream_handler);
			conn->mode = STREAM_MODE_RECV;
		}
		
	}
}

static struct ixev_ctx *stream_accept(struct ip_tuple *id)
{
	/* NOTE: we accept everything right now, did we want a port? */
	struct stream_conn *conn = mempool_alloc(&stream_conn_pool);
	if (!conn)
		return NULL;

	conn->mode = STREAM_MODE_RECV;
	ixev_ctx_init(&conn->ctx);
	ixev_set_handler(&conn->ctx, IXEVIN, &stream_handler);

	return &conn->ctx;
}

static void stream_release(struct ixev_ctx *ctx)
{
	struct stream_conn *conn = container_of(ctx, struct stream_conn, ctx);

	mempool_free(&stream_conn_pool, conn);
}

struct ixev_conn_ops stream_conn_ops = {
	.accept		= &stream_accept,
	.release	= &stream_release,
};

static void *stream_main(void *arg)
{
	int ret;

	ret = ixev_init_thread();
	if (ret) {
		printf("unable to init IXEV\n");
		return NULL;
	};

	ret = mempool_create(&stream_conn_pool, &stream_conn_datastore);
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
	int ret;
	int stream_conn_pool_entries;

	if (argc >= 2)
		stream_conn_pool_entries = atoi(argv[1]);
	else
		stream_conn_pool_entries = 16 * 4096;

	stream_conn_pool_entries = ROUND_UP(stream_conn_pool_entries, MEMPOOL_DEFAULT_CHUNKSIZE);

	ret = ixev_init(&stream_conn_ops);
	if (ret) {
		printf("failed to initialize ixev\n");
		return ret;
	}

	ret = mempool_create_datastore(&stream_conn_datastore, stream_conn_pool_entries, sizeof(struct stream_conn), 0, MEMPOOL_DEFAULT_CHUNKSIZE, "stream_conn");
	if (ret) {
		printf("unable to create mempool\n");
		return ret;
	}

	nr_cpu = sys_nrcpus();
	if (nr_cpu < 1) {
		printf("got invalid cpu count %d\n", nr_cpu);
		exit(-1);
	}
	nr_cpu--; /* don't count the main thread */

	sys_spawnmode(true);

	for (i = 0; i < nr_cpu; i++) {
		if (pthread_create(&tid, NULL, stream_main, NULL)) {
			printf("failed to spawn thread %d\n", i);
			exit(-1);
		}
	}

	stream_main(NULL);
	printf("exited\n");
	return 0;
}

