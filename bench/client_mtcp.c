#define _GNU_SOURCE

#include <mtcp_api.h>
#include <mtcp_epoll.h>

#include <arpa/inet.h>

#include <netinet/tcp.h>

#include <sys/prctl.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "client_common.h"
#include "timer.h"

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define MAX_CORES 128

#define MAX_FLOW_NUM  (10000)
#define MAX_EVENTS (MAX_FLOW_NUM * 3)

#define SOCKET_CTX_MAP_ENTRIES 16384

#define TIMEOUT_US 1000000l

#define MAX_ERRSOURCE 2
#define ERRSOURCE_CONNECT 0
#define ERRSOURCE_EVENT_ERROR 1

#define MAX_ERRNO 116

#define LOG_ERROR(worker, source, errno) \
	do { \
		if ((source) < 0 || (source) >= MAX_ERRSOURCE || (errno) < 0 || (errno) >= MAX_ERRNO) { \
			fprintf(stderr, "unhandled log_error(%d, %d)\n", (source), (errno)); \
			exit(1); \
		} \
		(worker)->errors[(source)][(errno)]++; \
	} while (0)

struct worker {
	int cpu;
	unsigned int connections;
	mctx_t mctx;
    int ep;
	pthread_t tid;
	unsigned char *buffer;
	unsigned int active_connections;
	unsigned long long total_connections;
	unsigned long long total_messages;
	unsigned int errors[MAX_ERRSOURCE][MAX_ERRNO];
	unsigned int timeouts_connect;
	unsigned int timeouts_recv;
	struct ctx *first_ctx;
    struct ctx **sctx_map;
} worker[MAX_CORES];

#define STATE_IDLE 1
#define STATE_CONNECTING 2
#define STATE_WAIT_FOR_RECV 3

#define UPDATE_STATE(ctx, new_state) \
	do { \
		(ctx)->state = (new_state); \
		(ctx)->timestamp = rdtsc(); \
	} while (0)

struct ctx {
	struct ctx *next;
	struct worker *worker;
	unsigned int state;
	unsigned long messages_left;
	unsigned int bytes_left;
	unsigned long timestamp;
	int sockid;
};

static struct sockaddr_in server_addr;
static int msg_size;
static long messages_per_connection;

static void setup_and_prune_connections(struct worker* worker);
static void new_connection(struct ctx *ctx);

static void close_mtcp_socket(struct ctx *ctx)
{
    if (ctx->state == STATE_WAIT_FOR_RECV)
        ctx->worker->active_connections--;
    
    mtcp_epoll_ctl(ctx->worker->mctx, ctx->worker->ep, MTCP_EPOLL_CTL_DEL, ctx->sockid, NULL);
    mtcp_close(ctx->worker->mctx, ctx->sockid);
    
    ctx->worker->sctx_map[ctx->sockid] = NULL;
    ctx->sockid = -1;
}

static void write_echo_message(struct ctx *ctx)
{
    int wrtot = 0;
        
    while (wrtot < msg_size) {
        int wr = mtcp_write(ctx->worker->mctx, ctx->sockid, (char *)&ctx->worker->buffer[wrtot], msg_size - wrtot);
        
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

static int mtcp_write_handler(struct ctx *ctx)
{
    struct mtcp_epoll_event evctl;
    int ret;
    
    if (ctx->state == STATE_CONNECTING) {
        ctx->worker->total_connections++;
		ctx->worker->active_connections++;
    }
    
    write_echo_message(ctx);
    
    evctl.events = MTCP_EPOLLIN | MTCP_EPOLLHUP | MTCP_EPOLLRDHUP;
    evctl.data.sockid = ctx->sockid;
    ret = mtcp_epoll_ctl(ctx->worker->mctx, ctx->worker->ep, MTCP_EPOLL_CTL_MOD, ctx->sockid, &evctl);
    if (ret < 0) {
        perror("epoll_ctl");
        exit(1);
    }
    
    UPDATE_STATE(ctx, STATE_WAIT_FOR_RECV);
    
    return 0;
}

static int mtcp_read_handler(struct ctx *ctx)
{
    int len, ret;
    char buf[8192];
    struct mtcp_epoll_event evctl;
    
    len = mtcp_read(ctx->worker->mctx, ctx->sockid, buf, sizeof(buf));
    if (len >= 0) {
        ctx->bytes_left -= len;
        if (!ctx->bytes_left) {
            ctx->worker->total_messages++;
            ctx->bytes_left = msg_size;
            if (!ctx->messages_left) {
                close_mtcp_socket(ctx);
                new_connection(ctx);
                return len;
            }
            ctx->messages_left--;
            
            evctl.events = MTCP_EPOLLOUT | MTCP_EPOLLHUP | MTCP_EPOLLRDHUP;
            evctl.data.sockid = ctx->sockid;
            ret = mtcp_epoll_ctl(ctx->worker->mctx, ctx->worker->ep, MTCP_EPOLL_CTL_MOD, ctx->sockid, &evctl);
            if (ret < 0) {
                perror("epoll_ctl");
                exit(1);
            }
        }
        UPDATE_STATE(ctx, STATE_WAIT_FOR_RECV);
    }
    
    return len;
}

static void mtcp_error_handler(struct ctx *ctx)
{
    close_mtcp_socket(ctx);
    UPDATE_STATE(ctx, STATE_IDLE);
    LOG_ERROR(ctx->worker, ERRSOURCE_EVENT_ERROR, errno);
}

static void new_connection(struct ctx *ctx)
{
	int ret;
    struct mtcp_epoll_event evctl;
    
    ctx->sockid = mtcp_socket(ctx->worker->mctx, AF_INET, MTCP_SOCK_STREAM, 0);
	if (ctx->sockid < 0) {
		perror("socket");
		exit(1);
	}
    
	ret = mtcp_setsock_nonblock(ctx->worker->mctx, ctx->sockid);
    if (ret < 0) {
        fprintf(stderr, "Error: Failed to make socket nonblocking on core %d\n", ctx->worker->cpu);
        exit(1);
    }
    
    ret = mtcp_connect(ctx->worker->mctx, ctx->sockid, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret < 0 && errno != EINPROGRESS) {
		ctx->sockid = -1;
        UPDATE_STATE(ctx, STATE_IDLE);
		LOG_ERROR(ctx->worker, ERRSOURCE_CONNECT, errno);
		return;
	}
	
	evctl.events = MTCP_EPOLLOUT | MTCP_EPOLLHUP | MTCP_EPOLLRDHUP;
    evctl.data.sockid = ctx->sockid;
    ret = mtcp_epoll_ctl(ctx->worker->mctx, ctx->worker->ep, MTCP_EPOLL_CTL_ADD, ctx->sockid, &evctl);
    if (ret < 0) {
        perror("epoll_ctl");
        exit(1);
    }
    
    UPDATE_STATE(ctx, STATE_CONNECTING);
    
    if (ctx->worker->sctx_map[ctx->sockid]) {
        fprintf(stderr, "Error: Replacing existing socket on core %d\n", ctx->worker->cpu);
        exit(1);
    }
    
    ctx->worker->sctx_map[ctx->sockid] = ctx;
    ctx->messages_left = messages_per_connection - 1;
}

static void setup_and_prune_connections(struct worker* worker)
{
    struct ctx *ctx;
	unsigned long deadline;

	deadline = rdtsc() - TIMEOUT_US * cycles_per_us;
	ctx = worker->first_ctx;

	while (ctx) {
		if (ctx->timestamp >= deadline) {
			ctx = ctx->next;
			continue;
		}
        
		if (ctx->state == STATE_IDLE) {
            new_connection(ctx);
		} else if (ctx->state == STATE_WAIT_FOR_RECV || ctx->state == STATE_CONNECTING) {
			
			switch (ctx->state) {
			case STATE_CONNECTING:
				ctx->worker->timeouts_connect++;
				break;
			case STATE_WAIT_FOR_RECV:
				ctx->worker->timeouts_recv++;
                exit(1);
				break;
			default:
				fprintf(stderr, "unreported timeout while at state %d\n.", ctx->state);
				exit(1);
			}
			close_mtcp_socket(ctx);
            new_connection(ctx);
		}

		ctx = ctx->next;
	}
}

static void mtcp_epoll_loop(struct worker* worker)
{
    struct mtcp_epoll_event *events;
    int nevents, i;
    
    events = (struct mtcp_epoll_event *)calloc(MAX_EVENTS, sizeof(struct mtcp_epoll_event));
    if (!events) {
        fprintf(stderr, "Error: Failed to create epoll event struct on core %d\n", worker->cpu);
        exit(1);
    }
    
    setup_and_prune_connections(worker);
    while (1) {
        nevents = mtcp_epoll_wait(worker->mctx, worker->ep, events, MAX_EVENTS, (TIMEOUT_US / 1000));
        if (nevents < 0) {
			if (errno != EINTR)
				perror("mtcp_epoll_wait");
			exit(1);
		}
        
        for (i = 0; i < nevents; ++i) {
            if (events[i].events & MTCP_EPOLLERR) {
                mtcp_error_handler(worker->sctx_map[events[i].data.sockid]);
            } else if (events[i].events & (MTCP_EPOLLHUP | MTCP_EPOLLRDHUP)) {
                fprintf(stderr, "Connection terminated prematurely.\n");
                exit(1);
            } else if (events[i].events & MTCP_EPOLLIN) {
                if (mtcp_read_handler(worker->sctx_map[events[i].data.sockid]) < 0 && errno != EAGAIN) {
                    mtcp_error_handler(worker->sctx_map[events[i].data.sockid]);
                }
            } else if (events[i].events & MTCP_EPOLLOUT) {
                if (mtcp_write_handler(worker->sctx_map[events[i].data.sockid]) < 0) {
                    mtcp_error_handler(worker->sctx_map[events[i].data.sockid]);
                }
            }
        }
        
        setup_and_prune_connections(worker);
    }
}

static void *start_worker(void *p)
{
	int i;
	struct worker *worker;
	struct ctx *ctx;

	worker = p;
    worker->sctx_map = (struct ctx **)calloc(SOCKET_CTX_MAP_ENTRIES, sizeof(struct ctx *));

	// affinitize application thread to a CPU core
    mtcp_core_affinitize(worker->cpu);

	worker->buffer = malloc(msg_size);
	for (i = 0; i < msg_size; i++)
		worker->buffer[i] = '0';
        
	// create mTCP context and spawn an mTCP thread
    worker->mctx = mtcp_create_context(worker->cpu);
	if (!worker->mctx) {
		fprintf(stderr, "Failed to create mTCP context on core %d\n", worker->cpu);
		exit(1);
	}
    
    // create an epoll descriptor
    worker->ep = mtcp_epoll_create(worker->mctx, MAX_EVENTS);
    if (worker->ep < 0) {
        fprintf(stderr, "Error: Failed to create epoll descriptor on core %d\n", worker->cpu);
        exit(EXIT_FAILURE);
	}

	worker->first_ctx = NULL;
	for (i = 0; i < worker->connections; i++) {
		ctx = worker->first_ctx;
		worker->first_ctx = malloc(sizeof(struct ctx));
		worker->first_ctx->next = ctx;
        worker->first_ctx->state = STATE_IDLE;
        worker->first_ctx->timestamp = 0;
		worker->first_ctx->worker = worker;
        worker->first_ctx->sockid = -1;
	}
    
    mtcp_epoll_loop(worker);

	return 0;
}

static int start_threads(unsigned int cores, unsigned int connections)
{
	int i;
	int connections_per_core = connections / cores;
	int leftover_connections = connections % cores;

	for (i = 0; i < cores; i++) {
		worker[i].cpu = i;
		worker[i].active_connections = 0;
		worker[i].total_connections = 0;
		worker[i].total_messages = 0;
		worker[i].connections = connections_per_core + (i < leftover_connections ? 1 : 0);
		pthread_create(&worker[i].tid, NULL, start_worker, &worker[i]);
	}
    
	return 0;
}

int main(int argc, char **argv)
{
	int i;
	int j;
	int k;
	int sum;
	int cores;
	int connections;
	long long total_connections;
	long long total_messages;
	int active_connections;
	int timeouts_connect;
	int timeouts_recv;
	char buf;
	int ret;
	char ifname[64];
	long rx_bytes, rx_packets, tx_bytes, tx_packets;

	prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0);

	if (argc != 7) {
		fprintf(stderr, "Usage: %s IP PORT CORES CONNECTIONS MSG_SIZE MESSAGES_PER_CONNECTION\n", argv[0]);
		return 1;
	}

	server_addr.sin_family = AF_INET;
	if (!inet_aton(argv[1], &server_addr.sin_addr)) {
		fprintf(stderr, "Invalid server IP address \"%s\".\n", argv[1]);
		return 1;
	}
	server_addr.sin_port = htons(atoi(argv[2]));
	cores = atoi(argv[3]);
	connections = atoi(argv[4]);
	msg_size = atoi(argv[5]);
	messages_per_connection = strtol(argv[6], NULL, 10);

	if (timer_calibrate_tsc()) {
		fprintf(stderr, "Error: Timer calibration failed.\n");
		return 1;
	}
    
    if (mtcp_init("client_mtcp.conf")) {
        fprintf(stderr, "Error: mTCP initialization failed.\n");
        return 1;
    }

	get_ifname(&server_addr, ifname);

	start_threads(cores, connections);
	puts("ok");
	fflush(stdout);

	while (1) {
		ret = read(STDIN_FILENO, &buf, 1);
		if (ret == 0) {
			fprintf(stderr, "Error: EOF on STDIN.\n");
			return 1;
		} else if (ret == -1) {
			perror("read");
			return 1;
		}
		get_eth_stats(ifname, &rx_bytes, &rx_packets, &tx_bytes, &tx_packets);
		total_connections = 0;
		total_messages = 0;
		active_connections = 0;
		timeouts_connect = 0;
		timeouts_recv = 0;
		for (i = 0; i < cores; i++) {
			total_connections += worker[i].total_connections;
			total_messages += worker[i].total_messages;
			active_connections += worker[i].active_connections;
			timeouts_connect += worker[i].timeouts_connect;
			timeouts_recv += worker[i].timeouts_recv;
		}
		printf("%lld %lld %d %d %d ", total_connections, total_messages, active_connections, timeouts_connect, timeouts_recv);
		printf("%ld %ld %ld %ld ", rx_bytes, rx_packets, tx_bytes, tx_packets);
		printf("0 ");
		for (i = 0; i < MAX_ERRSOURCE; i++) {
			for (j = 0; j < MAX_ERRNO; j++) {
				sum = 0;
				for (k = 0; k < cores; k++)
					sum += worker[k].errors[i][j];
				if (sum)
					printf("%d %d %d ", i, j, sum);
			}
		}

		puts("");
		fflush(stdout);
	}
    
    mtcp_destroy();
	return 0;
}
