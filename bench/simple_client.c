#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#define MAX_CORES 128
#define BUFFER_SIZE 65536
#define MAX_CONNECTIONS_PER_THREAD 65536

struct worker {
	pthread_t tid;
	char *buffer;
	unsigned long long total_connections;
	unsigned long long total_messages;
} worker[MAX_CORES];

static struct sockaddr_in server_addr;
static int msg_size;
static long messages_per_connection;
static int connections_per_thread;

int sock_connect(void)
{
	int fd;
	int flag;
	struct linger linger;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		exit(1);
	}

	linger.l_onoff = 1;
	linger.l_linger = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (void *) &linger, sizeof(linger))) {
		perror("setsockopt(SO_LINGER)");
		exit(1);
	}

	flag = 1;
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *) &flag, sizeof(flag))) {
		perror("setsockopt(TCP_NODELAY)");
		exit(1);
	}

	if (connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
		perror("connect");
		exit(1);
	}

	return fd;
}

void sock_send_recv(int fd, char *send_buffer)
{
	static char recv_buffer[BUFFER_SIZE];
	ssize_t ret;

	ret = send(fd, send_buffer, msg_size, 0);
	if (ret == -1) {
		perror("send");
		exit(1);
	} else if (ret != msg_size) {
		fprintf(stderr, "Sent only %ld bytes out of %d.\n", ret, msg_size);
		exit(1);
	}
	ret = recv(fd, recv_buffer, msg_size, 0);
	if (ret == -1) {
		perror("recv");
		exit(1);
	} else if (ret != msg_size) {
		fprintf(stderr, "Received only %ld bytes out of %d.\n", ret, msg_size);
		exit(1);
	}
#if 0
	if (memcmp(send_buffer, recv_buffer, msg_size)) {
		fprintf(stderr, "Send and recv data do not match.\n");
		exit(1);
	}
#endif
}

static void *start_worker(void *p)
{
	int i, j, rnd;
	struct worker *worker;
	int sock[MAX_CONNECTIONS_PER_THREAD];

	worker = p;

	worker->buffer = malloc(BUFFER_SIZE);
	rnd = rand();
	for (i = 0; i < msg_size; i++)
		worker->buffer[i] = 'A' + (i + rnd) % 26;

	while (1) {
		for (i = 0; i < connections_per_thread; i++) {
			sock[i] = sock_connect();
			worker[i].total_connections++;
		}

		for (j = 0; j < messages_per_connection; j++) {
			for (i = 0; i < connections_per_thread; i++) {
				sock_send_recv(sock[i], worker->buffer);
				worker->total_messages++;
			}
		}

		for (i = 0; i < connections_per_thread; i++) {
			close(sock[i]);
		}
	}

	return 0;
}

static int start_threads(unsigned int threads)
{
	int i;

	for (i = 0; i < threads; i++) {
		worker[i].total_connections = 0;
		worker[i].total_messages = 0;
		pthread_create(&worker[i].tid, NULL, start_worker, &worker[i]);
	}

	return 0;
}

int main(int argc, char **argv)
{
	int i;
	int threads;
	long long total_connections;
	long long total_messages;

	prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0);

	if (argc != 7) {
		fprintf(stderr, "Usage: %s IP PORT THREADS CONNECTIONS_PER_THREAD MSG_SIZE MESSAGES_PER_CONNECTION\n", argv[0]);
		return 1;
	}

	server_addr.sin_family = AF_INET;
	if (!inet_aton(argv[1], &server_addr.sin_addr)) {
		fprintf(stderr, "Invalid server IP address \"%s\".\n", argv[1]);
		return 1;
	}
	server_addr.sin_port = htons(atoi(argv[2]));
	threads = atoi(argv[3]);
	connections_per_thread = atoi(argv[4]);
	msg_size = atoi(argv[5]);
	messages_per_connection = strtol(argv[6], NULL, 10);

	if (msg_size > BUFFER_SIZE) {
		fprintf(stderr, "Error: MSG_SIZE (%d) is larger than maximum allowed (%d).\n", msg_size, BUFFER_SIZE);
		return 1;
	}

	if (connections_per_thread <= 0 || connections_per_thread > MAX_CONNECTIONS_PER_THREAD) {
		fprintf(stderr, "Error: Invalid CONNECTIONS_PER_THREAD.\n");
		return 1;
	}

	start_threads(threads);

	while (1) {
		sleep(1);
		total_connections = 0;
		total_messages = 0;
		for (i = 0; i < threads; i++) {
			total_connections += worker[i].total_connections;
			total_messages += worker[i].total_messages;
		}
		printf("%lld %lld %d %d %d\n", total_connections, total_messages, 0, 0, 0);
		fflush(stdout);
	}

	return 0;
}
