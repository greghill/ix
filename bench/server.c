#include "server.h"

struct ctx {
	struct worker *worker;
};

void echo_read_cb(struct bufferevent *bev, void *arg)
{
	long long len;
	struct evbuffer *input = bufferevent_get_input(bev);
	struct evbuffer *output = bufferevent_get_output(bev);

	len = evbuffer_get_length(input);
	if (len > 0)
		evbuffer_remove_buffer(input, output, len);
}

struct ctx *init_ctx(struct worker *worker)
{
	struct ctx *ctx;
	ctx = malloc(sizeof(struct ctx));
	ctx->worker = worker;
	return ctx;
}

int main(int argc, char **argv)
{
	int ret;

	init();

	ret = parse_cpus(argc > 1 ? argv[1] : NULL);
	if (ret)
		return ret;

	start_threads();

	pause();
	return 0;
}
