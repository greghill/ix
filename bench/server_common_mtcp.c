#include "server_mtcp.h"

#define SOCKET_CTX_MAP_ENTRIES 16384

struct worker {
	int cpu;
	unsigned long long total_connections;
	pthread_t tid;
	int enable;
    mctx_t mctx;
    int ep;
} worker[CORES];

static int accept_conn(struct worker *worker, struct ctx **sctx_map, int sock_listener)
{
    int sock_accepted;
    struct mtcp_epoll_event evctl;
    
    sock_accepted = mtcp_accept(worker->mctx, sock_listener, NULL, NULL);
    
    if (sock_accepted < 0) {
        if (errno != EAGAIN) {
            perror("accept");
            exit(1);
        }
    } else {
        // register the accepted socket for read and disconnect events
        evctl.events = MTCP_EPOLLIN | MTCP_EPOLLHUP | MTCP_EPOLLRDHUP;
        evctl.data.sockid = sock_accepted;
        mtcp_setsock_nonblock(worker->mctx, sock_accepted);
        mtcp_epoll_ctl(worker->mctx, worker->ep, MTCP_EPOLL_CTL_ADD, sock_accepted, &evctl);
        
        worker->total_connections++;
        sctx_map[sock_accepted] = init_ctx(worker->mctx);
    }
    
    return sock_accepted;
}

static void close_mtcp_socket(struct worker *worker, struct ctx **sctx_map, int sock)
{
    mtcp_epoll_ctl(worker->mctx, worker->ep, MTCP_EPOLL_CTL_DEL, sock, NULL);
    mtcp_close(worker->mctx, sock);
    free_ctx(sctx_map[sock]);
    sctx_map[sock] = NULL;
}

static void initialize_worker(struct worker *worker)
{
    // affinitize application thread to a CPU core
    mtcp_core_affinitize(worker->cpu);
    
    // create mTCP context and spawn an mTCP thread
    worker->mctx = mtcp_create_context(worker->cpu);
    if (!worker->mctx) {
        fprintf(stderr, "Error: Failed to create mTCP context on core %d\n", worker->cpu);
        exit(EXIT_FAILURE);
    }
    
    // create an epoll descriptor
    worker->ep = mtcp_epoll_create(worker->mctx, MAX_EVENTS);
    if (worker->ep < 0) {
        fprintf(stderr, "Error: Failed to create epoll descriptor on core %d\n", worker->cpu);
        exit(EXIT_FAILURE);
	}
}

static void *start_worker(void *p)
{
	struct worker *worker;
    struct ctx **sctx_map;
    struct mtcp_epoll_event *events;
    struct mtcp_epoll_event evctl;
	int nevents;
	struct sockaddr_in sin;
	int sock_listener;
	bool do_accept;
    int i;

	worker = p;
    sctx_map = (struct ctx **)calloc(SOCKET_CTX_MAP_ENTRIES, sizeof(struct ctx *));
    
	initialize_worker(worker);
    
    events = (struct mtcp_epoll_event *)calloc(MAX_EVENTS, sizeof(struct mtcp_epoll_event));
    if (!events) {
        fprintf(stderr, "Error: Failed to create epoll event struct on core %d\n", worker->cpu);
        exit(EXIT_FAILURE);
    }
    
    sock_listener = mtcp_socket(worker->mctx, AF_INET, MTCP_SOCK_STREAM, 0);
	if (sock_listener < 0) {
		perror("socket");
		exit(1);
	}
    
    if (mtcp_setsock_nonblock(worker->mctx, sock_listener) < 0) {
        fprintf(stderr, "Error: Failed to make socket nonblocking on core %d\n", worker->cpu);
        exit(EXIT_FAILURE);
    }
	
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(9876);
    
	if (mtcp_bind(worker->mctx, sock_listener, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
		perror("bind");
		exit(1);
	}

	if (mtcp_listen(worker->mctx, sock_listener, 4096) < 0) {
        perror("listen");
        exit(1);
    }
    
    evctl.events = MTCP_EPOLLIN;
    evctl.data.sockid = sock_listener;
    if (mtcp_epoll_ctl(worker->mctx, worker->ep, MTCP_EPOLL_CTL_ADD, sock_listener, &evctl) < 0) {
        perror("epoll_ctl");
        exit(1);
    }
    
    while (1) {
        nevents = mtcp_epoll_wait(worker->mctx, worker->ep, events, MAX_EVENTS, -1);
        if (nevents < 0) {
			if (errno != EINTR)
				perror("mtcp_epoll_wait");
			exit(1);
		}
        
        do_accept = false;
        for (i = 0; i < nevents; ++i) {
            if (events[i].data.sockid == sock_listener) {
                // accept the connection if the event is from the listener
                do_accept = true;
            } else if (events[i].events & MTCP_EPOLLERR) {
                // error on socket, bail
                fprintf(stderr, "Error: Socket error on core %d\n", worker->cpu);
                exit(1);
            } else if (events[i].events & MTCP_EPOLLHUP) {
                // unexpected close, just release the connection
                close_mtcp_socket(worker, sctx_map, events[i].data.sockid);
            } else if (events[i].events & MTCP_EPOLLRDHUP) {
                // peer closed the connection, so release it
                close_mtcp_socket(worker, sctx_map, events[i].data.sockid);
            } else if (events[i].events & MTCP_EPOLLIN) {
                // handle the read event and close the connection if there is a problem
                if (mtcp_read_handler(sctx_map[events[i].data.sockid], events[i].data.sockid) < 0 && errno != EAGAIN) {
                    close_mtcp_socket(worker, sctx_map, events[i].data.sockid);
                }
            } else {
                // should not happen, bail
                fprintf(stderr, "Error: Unrecognised socket event on core %d\n", worker->cpu);
                exit(1);
            }
        }
        
        // accept connections if the flag is set
        if (do_accept) {
            while (1) {
                if (accept_conn(worker, sctx_map, sock_listener) > 0) {
                    break;
                }
            }
        }
    }

	free((void *)sctx_map);
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
