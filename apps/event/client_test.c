#include <ix/stddef.h>
#include <ix/mempool.h>
#include <net/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ixev.h"

#define BUFSIZE	64

enum {
	CLIENT_MODE_RECV,
	CLIENT_MODE_SEND,
};

struct client_conn {
	struct ixev_ctx ctx;
	struct ip_tuple id;
	int mode;
	size_t bytes_recvd;
	size_t bytes_sent;
	char data[BUFSIZE];
};

static struct client_conn conn;

static void client_die(void)
{
	ixev_close(&conn.ctx);
	printf("remote connection was closed\n");
	exit(-1);
}

static void main_handler(struct ixev_ctx *ctx, unsigned int reason)
{
	ssize_t ret;

	if (conn.mode == CLIENT_MODE_SEND) {
		ret = ixev_send(ctx, &conn.data[conn.bytes_sent], BUFSIZE - conn.bytes_sent);
		if (ret <= 0) {
			if (ret != -EAGAIN)
				client_die();
			return;
		}

		conn.bytes_sent += ret;
		if (conn.bytes_sent < BUFSIZE)
			return;

		conn.bytes_recvd = 0;
		ixev_set_handler(ctx, IXEVIN, &main_handler);
	} else {
		ret = ixev_recv(ctx, &conn.data[conn.bytes_recvd], BUFSIZE - conn.bytes_recvd);
		if (ret <= 0) {
			if (ret != -EAGAIN)
				client_die();
			return;
		}

		conn.bytes_recvd += ret;
		if (conn.bytes_recvd < BUFSIZE)
			return;

		conn.bytes_sent = 0;
		ixev_set_handler(ctx, IXEVOUT, &main_handler);
	}
}

static struct ixev_ctx *client_accept(struct ip_tuple *id)
{
	return NULL;
}

static void client_release(struct ixev_ctx *ctx)
{
	struct client_conn *conn = container_of(ctx, struct client_conn, ctx);

	free(conn);
}

static void client_dialed(struct ixev_ctx *ctx, long ret)
{
	if (ret) {
		printf("failed to connect, ret = %ld\n", ret);
	}

	conn.mode = CLIENT_MODE_SEND;
	conn.bytes_sent = 0;

	ixev_set_handler(ctx, IXEVOUT, &main_handler);
	main_handler(&conn.ctx, IXEVOUT);
}

struct ixev_conn_ops stream_conn_ops = {
	.accept		= &client_accept,
	.release	= &client_release,
	.dialed		= &client_dialed,
};

static int parse_ip_addr(const char *str, uint32_t *addr)
{
	unsigned char a, b, c, d;

	if (sscanf(str, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) != 4)
		return -EINVAL;

	*addr = MAKE_IP_ADDR(a, b, c, d);
	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	if (argc != 3) {
		fprintf(stderr, "Usage: IP PORT\n");
		return -1;
	}

	if (parse_ip_addr(argv[1], &conn.id.dst_ip)) {
		fprintf(stderr, "Bad IP address '%s'", argv[1]);
		exit(1);
	}
	conn.id.dst_port = atoi(argv[2]);

	ixev_init(&stream_conn_ops);

	ret = ixev_init_thread();
	if (ret) {
		printf("unable to init IXEV\n");
		exit(ret);
	};

	ixev_dial(&conn.ctx, &conn.id);

	while (1) {
		ixev_wait();
	}

	printf("exited\n");
	return 0;
}

