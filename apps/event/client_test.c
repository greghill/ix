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

static struct client_conn *c;

static void client_die(void)
{
	ixev_close(&c->ctx);
	printf("remote connection was closed\n");
	exit(-1);
}

static void main_handler(struct ixev_ctx *ctx, unsigned int reason)
{
	ssize_t ret;

	if (c->mode == CLIENT_MODE_SEND) {
		ret = ixev_send(ctx, &c->data[c->bytes_sent], BUFSIZE - c->bytes_sent);
		if (ret <= 0) {
			if (ret != -EAGAIN)
				client_die();
			return;
		}

		c->bytes_sent += ret;
		if (c->bytes_sent < BUFSIZE)
			return;

		c->bytes_recvd = 0;
		ixev_set_handler(ctx, IXEVIN, &main_handler);
	} else {
		ret = ixev_recv(ctx, &c->data[c->bytes_recvd], BUFSIZE - c->bytes_recvd);
		if (ret <= 0) {
			if (ret != -EAGAIN)
				client_die();
			return;
		}

		c->bytes_recvd += ret;
		if (c->bytes_recvd < BUFSIZE)
			return;

		c->bytes_sent = 0;
		ixev_set_handler(ctx, IXEVOUT, &main_handler);
	}
}

static struct ixev_ctx *client_accept(struct ip_tuple *id)
{
	return NULL;
}

static void client_release(struct ixev_ctx *ctx)
{
	free(c);
}

static void client_dialed(struct ixev_ctx *ctx, long ret)
{
	if (ret) {
		printf("failed to connect, ret = %ld\n", ret);
	}

	c->mode = CLIENT_MODE_SEND;
	c->bytes_sent = 0;

	ixev_set_handler(ctx, IXEVOUT, &main_handler);
	main_handler(&c->ctx, IXEVOUT);
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

	c = malloc(sizeof(struct client_conn));
	if (!c)
		exit(-1);

	if (argc != 3) {
		fprintf(stderr, "Usage: IP PORT\n");
		return -1;
	}

	if (parse_ip_addr(argv[1], &c->id.dst_ip)) {
		fprintf(stderr, "Bad IP address '%s'", argv[1]);
		exit(1);
	}

	c->id.dst_port = atoi(argv[2]);

	ixev_init(&stream_conn_ops);

	ret = ixev_init_thread();
	if (ret) {
		printf("unable to init IXEV\n");
		exit(ret);
	};

	ixev_dial(&c->ctx, &c->id);

	while (1) {
		ixev_wait();
	}

	printf("exited\n");
	return 0;
}

