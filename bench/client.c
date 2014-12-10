#define _GNU_SOURCE

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/thread.h>

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

#define TIMEOUT_US 1000000l

#define MAX_ERRSOURCE 2
#define ERRSOURCE_CONNECT 0
#define ERRSOURCE_EVENT_ERROR 1

#define MAX_ERRNO 114

#define LOG_ERROR(worker, source, errno) \
	do { \
		if ((source) < 0 || (source) >= MAX_ERRSOURCE || (errno) < 0 || (errno) >= MAX_ERRNO) { \
                    fprintf(stderr, "unhandled log_error(%d, %d, %s)\n", (source), (errno), evutil_socket_error_to_string(errno)); \
			exit(1); \
		} \
		(worker)->errors[(source)][(errno)]++; \
	} while (0)

struct worker {
	int cpu;
	unsigned int connections;
	unsigned int finished;
	unsigned int slow_connections;
	unsigned int churn_per_second;
	struct event_base *base;
	pthread_t tid;
	char *buffer;
	unsigned int active_connections;
	unsigned long long total_fast_connections;
	unsigned long long total_slow_connections;
	unsigned long long total_fast_messages;
	unsigned long long total_slow_messages;
	unsigned int errors[MAX_ERRSOURCE][MAX_ERRNO];
	unsigned int timeouts_connect;
	unsigned int timeouts_recv;
	unsigned int timeouts_connect_slow;
	unsigned int timeouts_recv_slow;
	size_t fast_rate;
	struct ctx *first_ctx;

        struct bufferevent_rate_limit_group *slow_bucket;
        struct ev_token_bucket_cfg *slow_bucket_cfg;
        struct bufferevent_rate_limit_group *fast_bucket;
        struct ev_token_bucket_cfg *fast_bucket_cfg;
} worker[MAX_CORES];

#define STATE_IDLE 1
#define STATE_CONNECTING 2
#define STATE_WAIT_FOR_RECV 3
#define STATE_WAIT_FOR_SEND 4
#define STATE_DONE 5

#define UPDATE_STATE(ctx, new_state) \
	do { \
		(ctx)->state = (new_state); \
		(ctx)->timestamp = rdtsc(); \
	} while (0)

struct ctx {
	struct ctx *next;
	struct worker *worker;
	int no;
	unsigned int state;
	unsigned long messages_left;
	unsigned int bytes_left;
	unsigned long timestamp;
	struct bufferevent *bev;
	int flood;
	uint32_t msg_size;
        unsigned long sent_timestamp;
        struct event *event;
};


static struct sockaddr_in server_addr;
static long messages_per_connection;

static void new_connection(struct event_base *base, struct ctx *ctx);
static int send_next_msg(struct ctx *ctx);

static uint32_t msg_size;
static void echo_read_cb(struct bufferevent *bev, void *arg)
{
	struct ctx *ctx = arg;

	long long len;
	struct evbuffer *input = bufferevent_get_input(bev);

	len = evbuffer_get_length(input);
	evbuffer_drain(input, len);
	ctx->bytes_left -= len;
	if (!ctx->bytes_left) {
            if(ctx->flood) {
		ctx->worker->total_fast_messages++;
            } else {
		ctx->worker->total_slow_messages++;
            }

            send_next_msg(ctx);
	}
}


static int send_next_msg(struct ctx *ctx) {
    ctx->bytes_left = ctx->msg_size;

    if (!ctx->messages_left) {
        bufferevent_remove_from_rate_limit_group(ctx->bev);
        bufferevent_free(ctx->bev);
        if (ctx->state == STATE_WAIT_FOR_RECV) {
            UPDATE_STATE(ctx, STATE_DONE);
            ctx->worker->finished++;
            ctx->worker->active_connections--;
        }
        new_connection(ctx->worker->base, ctx);
        return 1;
    }

    ctx->messages_left--;
    //printf("worker: %d, ctx: %d, writing %u bytes\n", ctx->worker->cpu, ctx->no, ctx->bytes_left);
    // Use bytes_left in case msg_size is changed in another thread
    sprintf(ctx->worker->buffer, "%010u|", ctx->bytes_left);
    bufferevent_write(ctx->bev, ctx->worker->buffer, ctx->bytes_left);
    ctx->sent_timestamp = rdtsc();
    UPDATE_STATE(ctx, STATE_WAIT_FOR_RECV);
    return 0; //hack to preserve old behaviour
}

static void maintain_connections_cb(evutil_socket_t fd, short what, void *arg)
{
	struct worker *worker = arg;
	struct ctx *ctx;
	unsigned long deadline;

	deadline = rdtsc() - TIMEOUT_US * cycles_per_us;
	ctx = worker->first_ctx;
	unsigned int unflood  = worker->churn_per_second;
	unsigned int to_flood = worker->churn_per_second;
        struct evbuffer *b;

	while (ctx) {
		if(unflood > 0 && ctx->flood) {
			if(ctx->bev && bufferevent_add_to_rate_limit_group(ctx->bev, ctx->worker->slow_bucket)) {
				printf("Error on adding to rate limit slow\n");
			}
			ctx->flood = 0;
			unflood--;
			ctx->msg_size = 64;
		} else if(to_flood > 0 && ctx->flood == 0) {
			if(ctx->bev && bufferevent_add_to_rate_limit_group(ctx->bev, ctx->worker->fast_bucket)) {
				printf("Error on adding to rate limit fast\n");
			}
			ctx->flood = 1;
			to_flood--;
			ctx->msg_size = msg_size;
		}

		if (ctx->state == STATE_IDLE) {
			new_connection(worker->base, ctx);
			ctx = ctx->next;
			continue;
                }

		if (ctx->timestamp >= deadline) {
			ctx = ctx->next;
			continue;
		}

		if (ctx->state == STATE_WAIT_FOR_RECV || ctx->state == STATE_CONNECTING) {
                    if(!ctx->bev) {
                        continue;
                    }
                    bufferevent_remove_from_rate_limit_group(ctx->bev);
                    bufferevent_free(ctx->bev);

                    if (ctx->state == STATE_WAIT_FOR_RECV)
                        ctx->worker->active_connections--;

                    switch (ctx->state) {
                    case STATE_CONNECTING:
                        if(ctx->flood){
                            ctx->worker->timeouts_connect++;
                        } else {
                            ctx->worker->timeouts_connect_slow++;
                        }

                        break;
                    case STATE_WAIT_FOR_RECV:

                        b = bufferevent_get_output(ctx->bev);

                        if(ctx->flood){
                            ctx->worker->timeouts_recv++;
                        } else {
                            ctx->worker->timeouts_recv_slow++;
                        }
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
	        if(ctx->flood) {
			ctx->worker->total_fast_connections++;
		}
		else  {
			ctx->worker->total_slow_connections++;
		}
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

	if (events & BEV_EVENT_WRITING) {
            printf("WRITING_EVENT error\n");
		bufferevent_free(bev);
		if (ctx->state == STATE_WAIT_FOR_RECV)
			ctx->worker->active_connections--;
		UPDATE_STATE(ctx, STATE_IDLE);
		LOG_ERROR(ctx->worker, ERRSOURCE_EVENT_ERROR, EVUTIL_SOCKET_ERROR());
		return;
	}

	if (events & BEV_EVENT_READING) {
            printf("READING_EVENT error\n");
		bufferevent_free(bev);
		if (ctx->state == STATE_WAIT_FOR_RECV)
			ctx->worker->active_connections--;
		UPDATE_STATE(ctx, STATE_IDLE);
		LOG_ERROR(ctx->worker, ERRSOURCE_EVENT_ERROR, EVUTIL_SOCKET_ERROR());
		return;
	}

	if (events & BEV_EVENT_TIMEOUT) {
            printf("TIMEOUT_EVENT error\n");
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
        printf("uncaught event %d\n", events);
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
        if(ctx->flood) {
            if(bufferevent_add_to_rate_limit_group(ctx->bev, ctx->worker->fast_bucket)) {
		    printf("Error on adding to rate limit fast\n");
	    }
        } else {
            if(bufferevent_add_to_rate_limit_group(ctx->bev, ctx->worker->slow_bucket)) {
		    printf("Error on adding to rate limit slow\n");
	    }
        }


	//printf("worker: %d, ctx: %d, connecting with %u bytes\n", ctx->worker->cpu, ctx->no, ctx->msg_size);
	if (bufferevent_socket_connect(ctx->bev, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
                bufferevent_remove_from_rate_limit_group(ctx->bev);
		bufferevent_free(ctx->bev);
		UPDATE_STATE(ctx, STATE_IDLE);
		LOG_ERROR(ctx->worker, ERRSOURCE_CONNECT, EVUTIL_SOCKET_ERROR());
		return;
	}

	bufferevent_setcb(ctx->bev, echo_read_cb, NULL, echo_event_cb, ctx);
	bufferevent_enable(ctx->bev, EV_READ);

	ctx->messages_left = messages_per_connection - 1;
	ctx->bytes_left = ctx->msg_size;
        sprintf(ctx->worker->buffer, "%010u|", ctx->msg_size);
        ctx->sent_timestamp = rdtsc();
	bufferevent_write(ctx->bev, ctx->worker->buffer, ctx->bytes_left);
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
	struct timeval slow_rate_interval;
	struct timeval fast_rate_interval = {1,0};
        size_t slow_rate;

	worker = p;

	set_affinity(worker->cpu);

	//printf("starting worker with %d conns, %d slow, %d churn, %u fast rate\n", worker->connections, worker->slow_connections, worker->churn_per_second, worker->fast_rate);

	worker->buffer = malloc(msg_size);
	for (i = 0; i < msg_size; i++)
		worker->buffer[i] = '0';

	worker->base = event_base_new();
	if (!worker->base) {
		puts("Couldn't open event base");
		exit(1);
	}

        // set slow_rate of slow connections to ~1pps. Note: this includes the overhead of conn setup
        if(worker->slow_connections  == 0) {
           slow_rate_interval.tv_sec = 1;
           slow_rate_interval.tv_usec = 0;
           slow_rate = 64; // irrelevant
        } else if(worker->slow_connections < 1000) {
            slow_rate_interval.tv_sec = 0;
            slow_rate_interval.tv_usec = 1000000 / worker->slow_connections;
            slow_rate = 64; // this is ~1 pps
        } else {
            slow_rate_interval.tv_sec = 0;
            slow_rate_interval.tv_usec = 1000;
            slow_rate = (64*worker->slow_connections)/1000; // roughly 1pps
        }

        worker->slow_bucket_cfg = ev_token_bucket_cfg_new(EV_RATE_LIMIT_MAX, EV_RATE_LIMIT_MAX,
                                                  slow_rate, slow_rate, &slow_rate_interval); // 4000pps with 64b payloads
        worker->slow_bucket = bufferevent_rate_limit_group_new(worker->base,
                                                               worker->slow_bucket_cfg);
        worker->fast_bucket_cfg = ev_token_bucket_cfg_new(EV_RATE_LIMIT_MAX, EV_RATE_LIMIT_MAX,
                                                  worker->fast_rate, worker->fast_rate, &fast_rate_interval); // 4000pps with 64b payloads
        worker->fast_bucket = bufferevent_rate_limit_group_new(worker->base,
                                                               worker->fast_bucket_cfg);

	worker->first_ctx = NULL;
	for (i = 0; i < worker->connections; i++) {
		ctx = worker->first_ctx;
		worker->first_ctx = malloc(sizeof(struct ctx));
		worker->first_ctx->event = NULL;
                if (i < worker->slow_connections) {
                    worker->first_ctx->flood = 0;
                    worker->first_ctx->msg_size = 64;
                } else {
                    worker->first_ctx->flood = 1;
                    worker->first_ctx->msg_size = msg_size;
                }

		worker->first_ctx->next = ctx;
		UPDATE_STATE(worker->first_ctx, STATE_IDLE);
		worker->first_ctx->worker = worker;
	}

	event = event_new(worker->base, -1, EV_TIMEOUT | EV_PERSIST, maintain_connections_cb, worker);
	event_add(event, &tv);

	event_base_dispatch(worker->base);

	return 0;
}

static int start_threads(unsigned int cores, unsigned int connections, unsigned int slow_connections, unsigned int churn_per_second, size_t global_rate)
{
	int i;
	int connections_per_core = connections / cores;
	int slow_connections_per_core = slow_connections / cores;
	int churn_per_second_per_core = churn_per_second / cores;

	int leftover_connections = connections % cores;
	int leftover_slow_connections = slow_connections % cores;
	int leftover_churn_per_second = churn_per_second % cores;

	for (i = 0; i < cores; i++) {
		worker[i].cpu = i;
		worker[i].active_connections = 0;
		worker[i].total_fast_connections = 0;
		worker[i].total_slow_connections = 0;
		worker[i].total_fast_messages = 0;
		worker[i].total_slow_messages = 0;
		worker[i].finished = 0;
		worker[i].connections = connections_per_core + (i < leftover_connections ? 1 : 0);
		worker[i].fast_rate = global_rate*worker[i].connections*msg_size;
		worker[i].slow_connections = slow_connections_per_core + (i < leftover_slow_connections ? 1 : 0);
		worker[i].churn_per_second = churn_per_second_per_core + (i < leftover_churn_per_second ? 1 : 0);
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
	unsigned int slow_connections = 0;
	unsigned int churn_per_second = 0;
	long long total_fast_connections;
	long long total_slow_connections;
	long long total_fast_messages;
	long long total_slow_messages;
	long long finished;
	int active_connections;
	int timeouts_connect;
	int timeouts_recv;
	int timeouts_connect_slow;
	int timeouts_recv_slow;
	char buf;
	int ret;
	char ifname[64];
	long rx_bytes, rx_packets, tx_bytes, tx_packets;
        size_t global_rate = EV_RATE_LIMIT_MAX;

	prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0);

	if (argc < 7) {
		fprintf(stderr, "Usage: %s IP PORT CORES CONNECTIONS MSG_SIZE MESSAGES_PER_CONNECTION [SLOW_CONNECTIONS] [CHURN_PER_SECOND] [FAST_RATE (msg/second)]\n", argv[0]);
		return 1;
	}

	if (argc > 10) {
		fprintf(stderr, "Usage: %s IP PORT CORES CONNECTIONS MSG_SIZE MESSAGES_PER_CONNECTION [SLOW_CONNECTIONS] [CHURN_PER_SECOND] [FAST_RATE (msg/second)]\n", argv[0]);
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

        if (argc >= 8)
            slow_connections = atoi(argv[7]);

        if (argc >= 9)
            churn_per_second = atoi(argv[8]);

        if (argc >= 10)
            global_rate = atoi(argv[9]);


	if (timer_calibrate_tsc()) {
		fprintf(stderr, "Error: Timer calibration failed.\n");
		return 1;
	}

	get_ifname(&server_addr, ifname);

	evthread_use_pthreads();

	start_threads(cores, connections, slow_connections, churn_per_second, global_rate);
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
		total_fast_connections = 0;
		total_slow_connections = 0;
		total_fast_messages = 0;
		total_slow_messages = 0;
                finished = 0;
		active_connections = 0;
		timeouts_connect = 0;
		timeouts_recv = 0;
		timeouts_connect_slow = 0;
		timeouts_recv_slow = 0;
		for (i = 0; i < cores; i++) {
			total_fast_connections += worker[i].total_fast_connections;
			total_slow_connections += worker[i].total_slow_connections;
			total_fast_messages += worker[i].total_fast_messages;
			total_slow_messages += worker[i].total_slow_messages;
                        finished += worker[i].finished;
			active_connections += worker[i].active_connections;
			timeouts_connect += worker[i].timeouts_connect;
			timeouts_recv += worker[i].timeouts_recv;
			timeouts_connect_slow += worker[i].timeouts_connect_slow;
			timeouts_recv_slow += worker[i].timeouts_recv_slow;
		}

		printf("%lld %lld %lld %lld %lld %d %d %d %d %d ", finished, total_fast_connections, total_slow_connections, total_fast_messages, total_slow_messages, active_connections, timeouts_connect, timeouts_connect_slow, timeouts_recv, timeouts_recv_slow);
		//printf("finished:%lld fast_con:%lld slow_con:%lld fast_msg:%lld slow_msg:%lld act:%d tm_con:%d tm_con_slow:%d tm_rcv:%d tm_rcv_slow:%d ", finished, total_fast_connections, total_slow_connections, total_fast_messages, total_slow_messages, active_connections, timeouts_connect, timeouts_connect_slow, timeouts_recv, timeouts_recv_slow);
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

	return 0;
}
