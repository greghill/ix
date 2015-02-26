#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "client_common.h"
#include "histogram.h"
#include "timer.h"

#define MAX_CORES 128
#define MAX_CONNECTIONS_PER_THREAD 65536
#define WAIT_AFTER_ERROR_US 1000000

struct worker {
	pthread_t tid;
	unsigned long long total_connections;
	unsigned long long total_messages;
} worker[MAX_CORES];

struct sock {
	int fd;
	enum {
		SOCK_STATE_CLOSED,
		SOCK_STATE_CONNECTED,
	} state;
	long try_again_at;
	long message_count;
	char *send_buffer;
	char *recv_buffer;
};

static struct sockaddr_in server_addr;
static int msg_size;
static long messages_per_connection;
static int connections_per_thread;
static int exit_on_error;
static int send_delay;

static long time_in_us(void)
{
	return rdtsc() / cycles_per_us;
}

static int sock_connect(struct sock *sock)
{
	int fd;
	int flag;
	int ret;
	struct linger linger;

	//printf("sock_connect\n");

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		exit(1);
	}
	sock->fd = fd;

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

	//printf("AAA\n");

	ret = connect(fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

	//printf("SSS %d\n", ret);

	if (ret == -1 && errno == ECONNRESET) {
		if (exit_on_error) {
			perror("connect");
			exit(1);
		} else {
			//printf("??\n");
			return 1;
		}
	} else if (ret == -1) {
		perror("connect");
		exit(1);
	}

	//printf("SOCK_STATE_CONNECTED\n");
	sock->state = SOCK_STATE_CONNECTED;
	sock->message_count = 0;
	return 0;
}

static int sock_send_recv(struct sock *sock)
{
	ssize_t ret;
	int recved;
	unsigned long start;

	//printf("sock_send_recv\n");

	start = rdtsc();
	ret = send(sock->fd, sock->send_buffer, msg_size, 0);
	if (ret == -1 && errno == ECONNRESET) {
		if (exit_on_error) {
			perror("send");
			exit(1);
		} else {
			return 1;
		}
	} else if (ret == -1) {
		perror("send");
		exit(1);
	} else if (ret != msg_size) {
		fprintf(stderr, "Sent only %ld bytes out of %d.\n", ret, msg_size);
		exit(1);
	}
	recved = 0;
	while (recved < msg_size) {
		ret = recv(sock->fd, &sock->recv_buffer[recved], msg_size - recved, 0);
		if (ret == -1 && errno == ECONNRESET) {
			if (exit_on_error) {
				perror("recv");
				exit(1);
			} else {
				return 1;
			}
		} else if (ret == -1) {
			perror("recv");
			exit(1);
		}
		recved += ret;
	}
	histogram_record((rdtsc() - start) / cycles_per_us);
	if (recved != msg_size) {
		fprintf(stderr, "Received %d bytes instead of %d.\n", recved, msg_size);
		exit(1);
	}

	if (memcmp(sock->send_buffer, sock->recv_buffer, msg_size)) {
		fprintf(stderr, "Send and recv data do not match.\nSend: '%.*s'\nRecv: '%.*s'\n", msg_size, sock->send_buffer, msg_size, sock->recv_buffer);
		exit(1);
	}

	sock->message_count++;
	return 0;
}

static void sock_close(struct sock *sock)
{
	close(sock->fd);
	sock->state = SOCK_STATE_CLOSED;
}

static void sock_handle(long now, struct sock *sock, struct worker *worker)
{
	int ret;

	if (now < sock->try_again_at)
		return;
	sock->try_again_at = 0;

	//printf("sock_handle\n");

	switch (sock->state) {
	case SOCK_STATE_CLOSED:
		//printf("SOCK_STATE_CLOSED\n");
		ret = sock_connect(sock);
		if (ret) {
			sock_close(sock);
			sock->try_again_at = now + WAIT_AFTER_ERROR_US;
		} else {
			worker->total_connections++;
		}
		break;
	case SOCK_STATE_CONNECTED:
		//printf("SOCK_STATE_CONNECTED\n");
		ret = sock_send_recv(sock);
		if (ret) {
			sock_close(sock);
			sock->try_again_at = now + WAIT_AFTER_ERROR_US;
		} else {
			worker->total_messages++;
			if (sock->message_count == messages_per_connection)
				sock_close(sock);
		}
		if (send_delay)
			usleep(send_delay);
		break;
	}
}

static void *start_worker(void *p)
{
	int i, j, ofs, step;
	long now;
	struct worker *worker;
	struct sock sock[MAX_CONNECTIONS_PER_THREAD];

	worker = p;

	for (i = 0; i < connections_per_thread; i++) {
		sock[i].send_buffer = malloc(msg_size);
		sock[i].recv_buffer = malloc(msg_size);
		ofs = rand();
		step = rand() % 4 + 1;
		for (j = 0; j < msg_size; j++)
			sock[i].send_buffer[j] = 'A' + (j * step + ofs) % 26;
	}

	now = 0;
	while (1) {
		if (!send_delay)
			now = time_in_us();
		for (i = 0; i < connections_per_thread; i++) {
			if (send_delay)
				now = time_in_us();
			sock_handle(now, &sock[i], worker);
		}
	}

	return 0;
}

static int start_threads(unsigned int threads)
{
	int i;

	for (i = 0; i < threads; i++) {
		//printf("starting thread %d\n", i);
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
	char buf;
	int ret;
	char ifname[64];
	long rx_bytes, rx_packets, tx_bytes, tx_packets;
	long rx_bytes_init, rx_packets_init, tx_bytes_init, tx_packets_init;
	unsigned long start_time;
	unsigned long elapsed_time;
	long rx_bytes_diff;
	long tx_bytes_diff;
	float rx_throughput;
	float tx_throughput;

	prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0);

	if (argc != 9) {
		fprintf(stderr, "Usage: %s IP PORT THREADS CONNECTIONS_PER_THREAD MSG_SIZE MESSAGES_PER_CONNECTION EXIT_ON_ERROR SEND_DELAY\n", argv[0]);
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
	exit_on_error = atoi(argv[7]);
	send_delay = atoi(argv[8]);

	if (connections_per_thread <= 0 || connections_per_thread > MAX_CONNECTIONS_PER_THREAD) {
		fprintf(stderr, "Error: Invalid CONNECTIONS_PER_THREAD.\n");
		return 1;
	}

	get_ifname(&server_addr, ifname);

	//printf("msg_size %d\n", msg_size);
	//printf("ifname: %s\n", ifname);

	if (timer_calibrate_tsc()) {
		fprintf(stderr, "Error: Timer calibration failed.\n");
		return 1;
	}
	histogram_init();

	start_threads(threads);
	puts("ok");
	fflush(stdout);


	get_eth_stats(ifname, &rx_bytes_init, &rx_packets_init, &tx_bytes_init, &tx_packets_init);
	start_time = time_in_us();

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
		for (i = 0; i < threads; i++) {
			total_connections += worker[i].total_connections;
			total_messages += worker[i].total_messages;
		}

		elapsed_time = time_in_us() - start_time;
		rx_bytes_diff = rx_bytes - rx_bytes_init;
		tx_bytes_diff = tx_bytes - tx_bytes_init;
		rx_throughput = ((float)rx_bytes_diff / (float)elapsed_time) * 1000000.0f / 1048576.0f;
		tx_throughput = ((float)tx_bytes_diff / (float)elapsed_time) * 1000000.0f / 1048576.0f;

		printf("-----\n");
		printf("%lld %lld %d %d %d ", total_connections, total_messages, 0, 0, 0);
		printf("%ld %ld %ld %ld ", rx_bytes, rx_packets, tx_bytes, tx_packets);
		printf("%d ", (int) histogram_get_percentile(0.99));
		puts("");
		fflush(stdout);

		//printf("%lu %lu\n", rx_bytes_init, tx_bytes_init);

		//printf("%lu %lu\n", rx_bytes_diff, tx_bytes_diff);
		printf("%f %f\n", rx_throughput * 8, tx_throughput * 8);

		printf("====\n");
	}

	return 0;
}
