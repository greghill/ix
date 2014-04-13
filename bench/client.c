#define _GNU_SOURCE

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

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

#define BUFFER_SIZE 65536

#define TIMEOUT_US 1000000l

#define MAX_ERRSOURCE 2
#define ERRSOURCE_CONNECT 0
#define ERRSOURCE_EVENT_ERROR 1

#define MAX_ERRNO 114

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
	struct event_base *base;
	pthread_t tid;
	unsigned char *buffer;
	unsigned int active_connections;
	unsigned long long total_connections;
	unsigned long long total_messages;
	unsigned int errors[MAX_ERRSOURCE][MAX_ERRNO];
	unsigned int timeouts_connect;
	unsigned int timeouts_recv;
	struct ctx *first_ctx;
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
	struct bufferevent *bev;
};

static struct sockaddr_in server_addr;
static int msg_size;
static long messages_per_connection;

static void new_connection(struct event_base *base, struct ctx *ctx);

static void echo_read_cb(struct bufferevent *bev, void *arg)
{
	struct ctx *ctx = arg;

	long long len;
	struct evbuffer *input = bufferevent_get_input(bev);

	len = evbuffer_get_length(input);
	evbuffer_drain(input, len);
	ctx->bytes_left -= len;
	if (!ctx->bytes_left) {
		ctx->worker->total_messages++;
		ctx->bytes_left = msg_size;
		if (!ctx->messages_left) {
			bufferevent_free(bev);
			if (ctx->state == STATE_WAIT_FOR_RECV)
				ctx->worker->active_connections--;
			new_connection(ctx->worker->base, ctx);
			return;
		}
		ctx->messages_left--;
		bufferevent_write(bev, ctx->worker->buffer, msg_size);
	}
	UPDATE_STATE(ctx, STATE_WAIT_FOR_RECV);
}

static void maintain_connections_cb(evutil_socket_t fd, short what, void *arg)
{
	struct worker *worker = arg;
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
			new_connection(worker->base, ctx);
		} else if (ctx->state == STATE_WAIT_FOR_RECV || ctx->state == STATE_CONNECTING) {
			bufferevent_free(ctx->bev);
			if (ctx->state == STATE_WAIT_FOR_RECV)
				ctx->worker->active_connections--;
			switch (ctx->state) {
			case STATE_CONNECTING:
				ctx->worker->timeouts_connect++;
				break;
			case STATE_WAIT_FOR_RECV:
				ctx->worker->timeouts_recv++;
				break;
			default:
				fprintf(stderr, "unreported timeout while at state %d\n.", ctx->state);
				exit(1);
			}
			new_connection(worker->base, ctx);
		}

		ctx = ctx->next;
	}
}

static void echo_event_cb(struct bufferevent *bev, short events, void *arg)
{
	struct ctx *ctx = arg;

	if (events & BEV_EVENT_CONNECTED) {
		ctx->worker->total_connections++;
		ctx->worker->active_connections++;
		UPDATE_STATE(ctx, STATE_WAIT_FOR_RECV);
		return;
	}

	if (events & BEV_EVENT_ERROR) {
		bufferevent_free(bev);
		if (ctx->state == STATE_WAIT_FOR_RECV)
			ctx->worker->active_connections--;
		UPDATE_STATE(ctx, STATE_IDLE);
		LOG_ERROR(ctx->worker, ERRSOURCE_EVENT_ERROR, EVUTIL_SOCKET_ERROR());
		return;
	}

	if (events & BEV_EVENT_EOF) {
		fprintf(stderr, "Connection terminated prematurely.\n");
		exit(1);
	}
}

static void new_connection(struct event_base *base, struct ctx *ctx)
{
	int s;
	struct linger linger;
	int flag;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		perror("socket");
		exit(1);
	}

	linger.l_onoff = 1;
	linger.l_linger = 0;
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, (void *) &linger, sizeof(linger))) {
		perror("setsockopt(SO_LINGER)");
		exit(1);
	}

	flag = 1;
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(flag))) {
		perror("setsockopt(TCP_NODELAY)");
		exit(1);
	}

	evutil_make_socket_nonblocking(s);

	ctx->bev = bufferevent_socket_new(base, s, BEV_OPT_CLOSE_ON_FREE);
	if (bufferevent_socket_connect(ctx->bev, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
		bufferevent_free(ctx->bev);
		UPDATE_STATE(ctx, STATE_IDLE);
		LOG_ERROR(ctx->worker, ERRSOURCE_CONNECT, EVUTIL_SOCKET_ERROR());
		return;
	}
	bufferevent_setcb(ctx->bev, echo_read_cb, NULL, echo_event_cb, ctx);
	bufferevent_enable(ctx->bev, EV_READ);
	ctx->messages_left = messages_per_connection - 1;
	ctx->bytes_left = msg_size;
	bufferevent_write(ctx->bev, ctx->worker->buffer, msg_size);
	UPDATE_STATE(ctx, STATE_CONNECTING);
}

static void set_affinity(int cpu)
{
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(cpu, &cpu_set);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0) {
		perror("sched_setaffinity");
		exit(1);
	}
}

static void *start_worker(void *p)
{
	int i;
	struct worker *worker;
	struct ctx *ctx;
	struct event *event;
	struct timeval tv = {1, 0};

	worker = p;

	set_affinity(worker->cpu);

	worker->buffer = malloc(BUFFER_SIZE);
	for (i = 0; i < BUFFER_SIZE; i++)
		worker->buffer[i] = '0';

	worker->base = event_base_new();
	if (!worker->base) {
		puts("Couldn't open event base");
		exit(1);
	}

	worker->first_ctx = NULL;
	for (i = 0; i < worker->connections; i++) {
		ctx = worker->first_ctx;
		worker->first_ctx = malloc(sizeof(struct ctx));
		worker->first_ctx->next = ctx;
		UPDATE_STATE(worker->first_ctx, STATE_IDLE);
		worker->first_ctx->worker = worker;
	}

	event = event_new(worker->base, -1, EV_TIMEOUT | EV_PERSIST, maintain_connections_cb, worker);
	event_add(event, &tv);

	event_base_dispatch(worker->base);

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

	if (msg_size > BUFFER_SIZE) {
		fprintf(stderr, "Error: MSG_SIZE (%d) is larger than maximum allowed (%d).\n", msg_size, BUFFER_SIZE);
		return 1;
	}

	if (timer_calibrate_tsc()) {
		fprintf(stderr, "Error: Timer calibration failed.\n");
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

	return 0;
}
