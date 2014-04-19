#include "server_mtcp.h"

struct ctx {
	 mctx_t mctx;
};

int mtcp_read_handler(struct ctx *ctx, int sock_read)
{
    int rd, rdtot, wr, wrtot;
    char buf[4096];
    
    rd = mtcp_read(ctx->mctx, sock_read, buf, sizeof(buf));
    rdtot = rd;
    
    while (rd > 0) {
        wrtot = 0;
        
        while (wrtot < rd) {
            wr = mtcp_write(ctx->mctx, sock_read, buf, rd);
            
            if (wr >= 0) {
                wrtot += wr;
            } else if (wr == -1 && errno == EAGAIN) {
                continue;
            } else {
                perror("write");
                exit(1);
            }
        }
        
        rd = mtcp_read(ctx->mctx, sock_read, buf, sizeof(buf));
        rdtot += rd;
    }
    
    return rdtot;
}

struct ctx *init_ctx(mctx_t mctx)
{
	struct ctx *ctx;
	ctx = malloc(sizeof(struct ctx));
	ctx->mctx = mctx;
	return ctx;
}

void free_ctx(struct ctx *ctx)
{
    free((void *)ctx);
}

int main(int argc, char **argv)
{
	int ret;

	init();
    
    ret = mtcp_init("server_mtcp.conf");
    if (ret)
        return ret;

	ret = parse_cpus(argc > 1 ? argv[1] : NULL);
	if (ret)
		return ret;

	start_threads();

	pause();
    mtcp_destroy();
	return 0;
}
