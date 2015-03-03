#include "server.h"
#include <event2/thread.h>

struct ctx {
	struct worker *worker;
	size_t bytes_left;
	unsigned char *buffer;
	uint32_t msg_size;
};

#if 0
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
	printf("Received new msg with %010u msg size\n", ctx->msg_size);
        ctx->bytes_left = ctx->msg_size - 11;
	bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, ctx);
        echo_read_cb(bev,arg);
}
#endif

void msg_size_cb(struct bufferevent *bev, void *arg)
{
	struct ctx *ctx = arg;
	//size_t len;

    ctx->msg_size = 1024;
	ctx->buffer = realloc(ctx->buffer, ctx->msg_size);
	//printf("Received new msg with %010u msg size\n", ctx->msg_size);
    ctx->bytes_left = ctx->msg_size;
	bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, ctx);
    echo_read_cb(bev,arg);
}

void echo_read_cb(struct bufferevent *bev, void *arg)
{
	struct ctx *ctx = arg;
	size_t len;

	len = bufferevent_read(bev, &ctx->buffer[ctx->msg_size - ctx->bytes_left], ctx->bytes_left);
	//printf("Received %zu bytes with %zu bytes left of %010u msg size\n", len, ctx->bytes_left, ctx->msg_size);
	ctx->bytes_left -= len;
	if (!ctx->bytes_left) {
		ctx->bytes_left = ctx->msg_size;
		//printf("replying with %010u msg size\n", ctx->msg_size);
                ctx->worker->total_messages++;
		bufferevent_write(bev, ctx->buffer, ctx->msg_size);
		bufferevent_setcb(bev, msg_size_cb, NULL, echo_event_cb, ctx);
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
	char buf;
        int i;
        unsigned long long active_connections;
        unsigned long long total_messages;

	init();


	ret = parse_cpus(argc > 1 ? argv[1] : NULL);
	if (ret) {
		fprintf(stderr, "Usage: %s [CPUS]\n", argv[0]);
		return ret;
	}
	evthread_use_pthreads();

	start_threads();

	while (1) {
		ret = read(STDIN_FILENO, &buf, 1);
                active_connections = 0;
                total_messages = 0;
		if (ret == 0) {
			fprintf(stderr, "Error: EOF on STDIN.\n");
			return 1;
		} else if (ret == -1) {
			perror("read");
			return 1;
		}
		for (i = 0; i < CORES; i++) {
			active_connections += worker[i].active_connections;
			total_messages += worker[i].total_messages;
		}

		printf("%llu active connections,%llu total messages \n", active_connections, total_messages);

		puts("");
		fflush(stdout);
	}
	return 0;
}
