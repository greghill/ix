#include "server.h"

struct ctx {
	struct worker *worker;
	size_t bytes_left;
	unsigned char *buffer;
};

#define BUFFER_SIZE 65536

static size_t msg_size;

void echo_read_cb(struct bufferevent *bev, void *arg)
{
	struct ctx *ctx = arg;
	size_t len;

	len = bufferevent_read(bev, &ctx->buffer[msg_size - ctx->bytes_left], ctx->bytes_left);
	ctx->bytes_left -= len;
	if (!ctx->bytes_left) {
		ctx->bytes_left = msg_size;
		bufferevent_write(bev, ctx->buffer, msg_size);
	}
}

struct ctx *init_ctx(struct worker *worker)
{
	struct ctx *ctx;
	ctx = malloc(sizeof(struct ctx));
	ctx->worker = worker;
	ctx->bytes_left = msg_size;
	ctx->buffer = malloc(msg_size);
	return ctx;
}

int main(int argc, char **argv)
{
	int ret;

	init();

	if (argc < 2) {
		fprintf(stderr, "Usage: %s MSG_SIZE [CPUS]\n", argv[0]);
		return 1;
	}

	msg_size = atoi(argv[1]);
	if (msg_size > BUFFER_SIZE) {
		fprintf(stderr, "Error: MSG_SIZE (%ld) is larger than maximum allowed (%d).\n", msg_size, BUFFER_SIZE);
		return 1;
	}

	ret = parse_cpus(argc > 2 ? argv[2] : NULL);
	if (ret)
		return ret;

	start_threads();

	pause();
	return 0;
}
