#include "server_mtcp.h"

struct ctx {
	mctx_t mctx;
	size_t bytes_left;
	unsigned char *buffer;
};

static size_t msg_size;

int mtcp_read_handler(struct ctx *ctx, int sock_read)
{
    int len = 0;
    int rdtot = 0;
    int wr, wrtot;
    
    if (ctx->bytes_left) {
        len = mtcp_read(ctx->mctx, sock_read, (char *)&ctx->buffer[msg_size - ctx->bytes_left], ctx->bytes_left);
        rdtot = len;
    }
    
    while (len > 0 && ctx->bytes_left) {
        if (len > ctx->bytes_left) {
            fprintf(stderr, "Error: Received too many bytes: got %d, expecting %lu\n", len, ctx->bytes_left);
            exit(EXIT_FAILURE);
        }
        
        ctx->bytes_left -= len;
        len = mtcp_read(ctx->mctx, sock_read, (char *)&ctx->buffer[msg_size - ctx->bytes_left], ctx->bytes_left);
        rdtot += len;
    }
    
    if (!ctx->bytes_left) {
        wrtot = 0;
        
        while (wrtot < msg_size) {
            wr = mtcp_write(ctx->mctx, sock_read, (char *)&ctx->buffer[wrtot], msg_size - wrtot);
            
            if (wr >= 0) {
                wrtot += wr;
            } else if (wr == -1 && errno == EAGAIN) {
                continue;
            } else {
                perror("write");
                exit(1);
            }
        }
        
        ctx->bytes_left = msg_size;
    }
    
    return rdtot;
}

struct ctx *init_ctx(mctx_t mctx)
{
	struct ctx *ctx;
	ctx = malloc(sizeof(struct ctx));
	ctx->mctx = mctx;
	ctx->bytes_left = msg_size;
	ctx->buffer = malloc(msg_size);
	return ctx;
}

void free_ctx(struct ctx *ctx)
{
    free((void *)ctx);
}

int main(int argc, char **argv)
{
	int ret;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s MSG_SIZE [CPUS]\n", argv[0]);
		return 1;
	}
    
    init();
    
    ret = mtcp_init("pingpongs_mtcp.conf");
    if (ret)
        return ret;

	msg_size = atoi(argv[1]);

	ret = parse_cpus(argc > 2 ? argv[2] : NULL);
	if (ret)
		return ret;

	start_threads();

	pause();
    mtcp_destroy();
	return 0;
}
