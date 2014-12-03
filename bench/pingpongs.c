#include "server.h"
#include <event2/thread.h>

struct ctx {
	struct worker *worker;
	size_t bytes_left;
	unsigned char *buffer;
	uint32_t msg_size;
};

void msg_size_cb(struct bufferevent *bev, void *arg)
{
	struct ctx *ctx = arg;
	size_t len;

	len = bufferevent_read(bev, ctx->buffer, 11);
        if(len < 11) {
            printf("error: did not receive complete msg_size information\n");
        }
        sscanf((char *)ctx->buffer, "%010u|", &ctx->msg_size);
	ctx->buffer = realloc(ctx->buffer, ctx->msg_size);
        ctx->bytes_left = ctx->msg_size;
	bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, ctx);
        echo_read_cb(bev,arg);
}

void echo_read_cb(struct bufferevent *bev, void *arg)
{
	struct ctx *ctx = arg;
	size_t len;

	len = bufferevent_read(bev, &ctx->buffer[ctx->msg_size - ctx->bytes_left], ctx->bytes_left);
	ctx->bytes_left -= len;
	if (!ctx->bytes_left) {
		ctx->bytes_left = ctx->msg_size;
		bufferevent_write(bev, ctx->buffer, ctx->msg_size);
	}
}

struct ctx *init_ctx(struct worker *worker)
{
	struct ctx *ctx;
	ctx = malloc(sizeof(struct ctx));
	ctx->worker = worker;
	ctx->bytes_left = ctx->msg_size;
	ctx->buffer = malloc(10);
	return ctx;
}

int main(int argc, char **argv)
{
	int ret;

	init();


	ret = parse_cpus(argc > 1 ? argv[1] : NULL);
	if (ret) {
		fprintf(stderr, "Usage: %s [CPUS]\n", argv[0]);
		return ret;
	}
	evthread_use_pthreads();

	start_threads();

	pause();
	return 0;
}
