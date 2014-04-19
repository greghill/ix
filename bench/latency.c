#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "histogram.h"
#include "timer.h"

#define BUFFER_SIZE 64
#define MAX_PERCENTILES 64

static char buffer[BUFFER_SIZE];

static struct sockaddr_in server_addr;
static int probes_per_connection;
static int interval_ms;

static void print_percentiles(double *percentiles, int percentiles_count)
{
	int i;

	printf("%d ", histogram_get_count());
	for (i = 0; i < percentiles_count; i++)
		printf("%f ", histogram_get_percentile(percentiles[i]));
	puts("");
}

static int probe(int sock)
{
	int len;
	int recv_count;
	long start;

	start = rdtsc();
	send(sock, buffer, sizeof(buffer), 0);

	recv_count = 0;
	while ((len = sizeof(buffer) - recv_count) > 0) {
		len = recv(sock, buffer, len, 0);
		if (len == -1 && (errno == EAGAIN || errno == ECONNRESET)) {
			sleep(1);
			return 1;
		} else if (len == -1) {
			perror("recv");
			exit(1);
		}
		recv_count += len;
	}
	histogram_record((rdtsc() - start) / cycles_per_us);
	return 0;
}

static void run(struct sockaddr_in *server_addr, int probes_per_connection, int interval_ms)
{
	int i;
	int sock;
	int ret;
	struct linger linger;
	struct timeval timeout;

	sock = socket(AF_INET, SOCK_STREAM, 0);

	linger.l_onoff = 1;
	linger.l_linger = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger))) {
		perror("setsockopt(SO_LINGER)");
		exit(1);
	}

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout))) {
		perror("setsockopt(SO_RCVTIMEO)");
		exit(1);
	}

	if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout))) {
		perror("setsockopt(SO_SNDTIMEO)");
		exit(1);
	}

	ret = connect(sock, (struct sockaddr *) server_addr, sizeof(struct sockaddr_in));
	if (ret && (errno == EINPROGRESS || errno == EHOSTUNREACH)) {
		sleep(1);
		goto out;
	} else if (ret) {
		perror("connect");
		exit(1);
	}

	for (i = 0; i < probes_per_connection; i++) {
		if (probe(sock))
			goto out;
		usleep(interval_ms * 1000);
	}

out:
	close(sock);
}

static void *start_worker(void *p)
{
	while (1)
		run(&server_addr, probes_per_connection, interval_ms);

	return NULL;
}

int main(int argc, char **argv)
{
	double percentiles[MAX_PERCENTILES];
	int i;
	int percentiles_count;
	pthread_t tid;

	prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0);

	if (argc < 6) {
		fprintf(stderr, "Usage: %s IP PORT PROBES_PER_CONNECTION INTERVAL_MS PERCENTILE...\n", argv[0]);
		return 1;
	}

	server_addr.sin_family = AF_INET;
	if (!inet_aton(argv[1], &server_addr.sin_addr)) {
		fprintf(stderr, "Invalid server IP address \"%s\".\n", argv[1]);
		return 1;
	}
	server_addr.sin_port = htons(atoi(argv[2]));
	probes_per_connection = atoi(argv[3]);
	interval_ms = atoi(argv[4]);
	for (i = 5; i < argc; i++)
		percentiles[i - 5] = atof(argv[i]) / 100;
	percentiles_count = argc - 5;

	if (timer_calibrate_tsc()) {
		fprintf(stderr, "Error: Timer calibration failed.\n");
		return 1;
	}
	histogram_init();

	for (i = 0; i < sizeof(buffer); i++)
		buffer[i] = '0';

	pthread_create(&tid, NULL, start_worker, NULL);

	while (1) {
		sleep(1);
		print_percentiles(percentiles, percentiles_count);
		fflush(stdout);
	}
}
