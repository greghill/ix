#include "server.h"
#include <event2/bufferevent.h>
#define MAX_ERRSOURCE 2
#define ERRSOURCE_EVENT_ERROR 1
#define MAX_ERRNO 114

#define LOG_ERROR(source, errno) \
	do { \
            fprintf(stderr, "unhandled log_error(%d, %d, %s)\n", (source), (errno), evutil_socket_error_to_string(errno)); \
	} while (0)

struct ctx {
	struct worker *worker;
	size_t bytes_left;
	unsigned char *buffer;
	uint32_t msg_size;
};


void echo_event_cb(struct bufferevent *bev, short events, void *arg)
{
	struct ctx *ctx = arg;
	if (events & (BEV_EVENT_EOF)) {
            ctx->worker->active_connections--;
		bufferevent_free(bev);
	}

	if (events & BEV_EVENT_ERROR) {
            if(EVUTIL_SOCKET_ERROR() != 104) { // 104:reset by peer, i.e., intentional
		LOG_ERROR(ERRSOURCE_EVENT_ERROR, EVUTIL_SOCKET_ERROR());
            }
            ctx->worker->active_connections--;
		bufferevent_free(bev);
		return;
	}

	if (events & BEV_EVENT_WRITING) {
            printf("WRITING_EVENT error\n");
		return;
	}

	if (events & BEV_EVENT_READING) {
            printf("READING_EVENT error\n");
		return;
	}

	if (events & BEV_EVENT_TIMEOUT) {
            printf("TIMEOUT_EVENT error\n");
		return;
	}

        printf("uncaught event %d\n", events);
}

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
	//printf("Received new msg with %010u msg size\n", ctx->msg_size);
        ctx->bytes_left = ctx->msg_size - 11;
	bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, ctx);
        echo_read_cb(bev,arg);
}
#endif

static void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *arg)
{
	struct bufferevent *bev;
	struct ctx *ctx;
	struct worker *worker = arg;
	int flag;

	flag = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(flag))) {
		perror("setsockopt(TCP_NODELAY)");
		exit(1);
	}

	worker->total_connections++;
	worker->active_connections++;
	bev = bufferevent_socket_new(worker->base, fd, BEV_OPT_CLOSE_ON_FREE);
	ctx = init_ctx(worker);
	bufferevent_setcb(bev, msg_size_cb, NULL, echo_event_cb, ctx);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}

static void accept_error_cb(struct evconnlistener *listener, void *arg)
{
	struct event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	fprintf(stderr, "Got an error %d (%s) on the listener. Shutting down.\n", err, evutil_socket_error_to_string(err));

	event_base_loopexit(base, NULL);
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
	struct worker *worker;
	struct evconnlistener *listener;
	struct sockaddr_in sin;
	int sock;
	int one;

	worker = p;

	set_affinity(worker->cpu);

	worker->base = event_base_new();
	if (!worker->base) {
		puts("Couldn't open event base");
		exit(1);
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (!sock) {
		perror("socket");
		exit(1);
	}

	evutil_make_socket_nonblocking(sock);

	one = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (void *) &one, sizeof(one))) {
		perror("setsockopt(SO_REUSEPORT)");
		exit(1);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(0);
	sin.sin_port = htons(9876);

	if (bind(sock, (struct sockaddr*)&sin, sizeof(sin))) {
		perror("bind");
		exit(1);
	}

	listener = evconnlistener_new(worker->base, accept_conn_cb, worker, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1, sock);
	if (!listener) {
		perror("Couldn't create listener");
		exit(1);
	}
	evconnlistener_set_error_cb(listener, accept_error_cb);

	event_base_dispatch(worker->base);

	return 0;
}

int start_threads(void)
{
	int i;

	for (i = 0; i < CORES; i++) {
		if (!worker[i].enable)
			continue;
		worker[i].cpu = i;
		pthread_create(&worker[i].tid, NULL, start_worker, &worker[i]);
	}

	return 0;
}

static void ignore_sigpipe(void)
{
	// ignore SIGPIPE (or else it will bring our program down if the client
	// closes its socket).
	// NB: if running under gdb, you might need to issue this gdb command:
	// handle SIGPIPE nostop noprint pass
	// because, by default, gdb will stop our program execution (which we
	// might not want).
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;

	if (sigemptyset(&sa.sa_mask) < 0 || sigaction(SIGPIPE, &sa, 0) < 0) {
		perror("Could not ignore the SIGPIPE signal");
		exit(EXIT_FAILURE);
	}
}

void init(void)
{
	prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0);
	ignore_sigpipe();
}

int parse_cpus(char *cpus)
{
	int cpu_count;
	int i;
	int val;
	char *tok;

	cpu_count = sysconf(_SC_NPROCESSORS_CONF);
	if (cpu_count > CORES) {
		fprintf(stderr, "Error: You have %d CPUs. The maximum supported number is %d.'\n", cpu_count, CORES);
		return 1;
	}

	if (cpus) {
		tok = strtok(cpus, ",");
		while (tok) {
			val = atoi(tok);
			if (val < 0 || val >= cpu_count) {
				fprintf(stderr, "Error: Invalid CPU specified '%s'\n", tok);
				return 1;
			}
			worker[val].enable = 1;

			tok = strtok(NULL, ",");
		}
	} else {
		for (i = 0; i < cpu_count; i++)
			worker[i].enable = 1;
	}

	return 0;
}
