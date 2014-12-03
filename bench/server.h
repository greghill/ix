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

#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define CORES 32

#define SO_REUSEPORT 15

struct ctx;
struct worker;

void init(void);
int parse_cpus(char *cpus);
int start_threads(void);
void echo_read_cb(struct bufferevent *bev, void *arg);
void msg_size_cb(struct bufferevent *bev, void *arg);
struct ctx *init_ctx(struct worker *worker);
void echo_event_cb(struct bufferevent *bev, short events, void *arg);
