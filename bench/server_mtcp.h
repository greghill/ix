#define _GNU_SOURCE

#include <mtcp_api.h>
#include <mtcp_epoll.h>

#include <arpa/inet.h>

#include <netinet/tcp.h>

#include <sys/prctl.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define CORES 32

#define MAX_FLOW_NUM  (10000)
#define MAX_EVENTS (MAX_FLOW_NUM * 3)

#define SO_REUSEPORT 15

struct worker {
	int cpu;
	unsigned long long total_connections;
	pthread_t tid;
	int enable;
    mctx_t mctx;
    int ep;
};

void init(void);
int parse_cpus(char *cpus);
int start_threads(void);
void mtcp_read_handler(struct worker *worker, int sock_read);
