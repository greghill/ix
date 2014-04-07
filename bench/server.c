#define _GNU_SOURCE

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <arpa/inet.h>

#include <sys/prctl.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define CORES 32

#define SO_REUSEPORT 15

struct worker {
	int cpu;
	unsigned long long total_connections;
	struct event_base *base;
	pthread_t tid;
	int enable;
} worker[CORES];

struct ctx {
	struct worker *worker;
};

static void echo_read_cb(struct bufferevent *bev, void *arg)
{
	long long len;
	struct evbuffer *input = bufferevent_get_input(bev);
	struct evbuffer *output = bufferevent_get_output(bev);

	len = evbuffer_get_length(input);
	if (len > 0)
		evbuffer_remove_buffer(input, output, len);
}

static void echo_event_cb(struct bufferevent *bev, short events, void *arg)
{
	if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		bufferevent_free(bev);
	}
}

static void accept_conn_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *arg)
{
	struct bufferevent *bev;
	struct ctx *ctx;
	struct worker *worker = arg;

	worker->total_connections++;
	bev = bufferevent_socket_new(worker->base, fd, BEV_OPT_CLOSE_ON_FREE);
	ctx = malloc(sizeof(struct ctx));
	ctx->worker = worker;
	bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, ctx);
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

static int start_threads(void)
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

int main(int argc, char **argv)
{
	int cpu_count;
	int val;
	int i;
	char *tok;

	prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0);
	ignore_sigpipe();

	cpu_count = sysconf(_SC_NPROCESSORS_CONF);
	if (cpu_count > CORES) {
		fprintf(stderr, "Error: You have %d CPUs. The maximum supported number is %d.'\n", cpu_count, CORES);
		return 1;
	}

	if (argc > 1) {
		tok = strtok(argv[1], ",");
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

	start_threads();

	pause();
	return 0;
}
