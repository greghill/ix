#include "server_mtcp.h"

void mtcp_read_handler(struct worker *worker, int sock_read)
{
    int rd, wr, wrtot;
    char buf[4096];
    
    rd = mtcp_read(worker->mctx, sock_read, buf, sizeof(buf));
    
    while (rd > 0) {
        wrtot = 0;
        
        while (wrtot < rd) {
            wr = mtcp_write(worker->mctx, sock_read, buf, rd);
            
            if (wr >= 0) {
                wrtot += wr;
            } else if (wr == -1 && errno == EAGAIN) {
                continue;
            } else {
                perror("write");
                exit(1);
            }
        }
        
        rd = mtcp_read(worker->mctx, sock_read, buf, sizeof(buf));
    }
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
