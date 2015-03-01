#include <ix/stddef.h>
#include <mempool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "ixev.h"

#define ROUND_UP(num, multiple) ((((num) + (multiple) - 1) / (multiple)) * (multiple));

struct pp_conn {
	struct ixev_ctx ctx;
	size_t bytes_left;
	char data[];
};

static size_t msg_size;

static struct mempool_datastore pp_conn_datastore;
static __thread struct mempool pp_conn_pool;

//static void pp_main_handler(struct ixev_ctx *ctx, unsigned int reason);

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
		//ixev_set_handler(ctx, IXEVIN, &pp_main_handler);
	}
}

#define SEND_BUFFER_SIZE	(2048)

static void pp_main_handler_in(struct ixev_ctx *ctx, unsigned int reason);

static void pp_main_handler_out(struct ixev_ctx *ctx, unsigned int reason)
{
	struct pp_conn *conn = container_of(ctx, struct pp_conn, ctx);
	ssize_t ret;
	//char moo[1];

#if 0
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
#endif

	//printf("pp_main_handler_out\n");

	while (1) {
		ret = ixev_send(ctx, &conn->data[0], SEND_BUFFER_SIZE);

		if(ret < 0)
		{
			ixev_set_handler(ctx, IXEVOUT, &pp_main_handler_out);
			return;
		}
	}
}

static void pp_main_handler_in(struct ixev_ctx *ctx, unsigned int reason)
{
	struct pp_conn *conn = container_of(ctx, struct pp_conn, ctx);
	ssize_t ret;
	char moo[1];	

	//printf("pp_main_handler_in\n");
	ret = ixev_recv(ctx, moo, 1);
	//printf("ixev_recv %lu %d\n", ret, (int)ret);

	while (1) {
		ret = ixev_send(ctx, &conn->data[0], SEND_BUFFER_SIZE);
		//printf("ixev_send %lu %d\n", ret, (int)ret);

		if(ret < 0)
		{
			ixev_set_handler(ctx, IXEVOUT, &pp_main_handler_out);
			return;
		}
	}
}

static struct ixev_ctx *pp_accept(struct ip_tuple *id)
{
	/* NOTE: we accept everything right now, did we want a port? */
	struct pp_conn *conn = mempool_alloc(&pp_conn_pool);
	if (!conn)
		return NULL;

	//printf("MOO\n");

	conn->bytes_left = 9999999999;
	ixev_ctx_init(&conn->ctx);
	//ixev_set_handler(&conn->ctx, IXEVOUT, &pp_main_handler_out);
	ixev_set_handler(&conn->ctx, IXEVIN, &pp_main_handler_in);

	return &conn->ctx;
}

static void pp_release(struct ixev_ctx *ctx)
{
	struct pp_conn *conn = container_of(ctx, struct pp_conn, ctx);

	mempool_free(&pp_conn_pool, conn);
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

	ret = mempool_create(&pp_conn_pool, &pp_conn_datastore);
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
	unsigned int pp_conn_pool_entries;

	/*
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [MAX_CONNECTIONS]\n", argv[0]);
		return -1;
	}
	
	msg_size = SEND_BUFFER_SIZE;

	if (argc >= 3)
		pp_conn_pool_entries = atoi(argv[1]);
	else
		pp_conn_pool_entries = 16 * 4096;
	*/


	msg_size = SEND_BUFFER_SIZE;

	pp_conn_pool_entries = 16 * 4096;

	pp_conn_pool_entries = ROUND_UP(pp_conn_pool_entries, MEMPOOL_DEFAULT_CHUNKSIZE);

	ret = ixev_init(&pp_conn_ops);
	if (ret) {
		printf("failed to initialize ixev\n");
		return ret;
	}

	ret = mempool_create_datastore(&pp_conn_datastore, pp_conn_pool_entries, sizeof(struct pp_conn) + msg_size, 0, MEMPOOL_DEFAULT_CHUNKSIZE, "pp_conn");
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
		if (pthread_create(&tid, NULL, pp_main, NULL)) {
			printf("failed to spawn thread %d\n", i);
			exit(-1);
		}
	}

	pp_main(NULL);
	printf("exited\n");
	return 0;
}

