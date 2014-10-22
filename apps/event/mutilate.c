#include <ix/stddef.h>
#include <mempool.h>
#include <net/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "ixev.h"

/* WARNING: adjust these constants for your system */
#define N	100000
#define TSC_RATE 3100


#define CMD_GET		0x00
#define CMD_SET		0x01
#define CMD_SASL	0x21

#define RESP_OK		0x00
#define RESP_SASL_ERR	0x20

static uint64_t samples[N];
static int sample_pos = N;

typedef struct __attribute__ ((__packed__)) {
	uint8_t magic;
	uint8_t opcode;
	uint16_t key_len;

	uint8_t extra_len;
	uint8_t data_type;
	union {
		uint16_t vbucket; // request use
		uint16_t status;  // response use
	};

	uint32_t body_len;
	uint32_t opaque;
	uint64_t version;

	// Used for set only.
	//uint64_t extras;

	char buf[4096];
} binary_header_t;

#define HEADER_LEN	24

enum {
	CLIENT_RECV_HDR,
	CLIENT_RECV_BODY,
	CLIENT_SEND,
};

struct client_conn {
	int state;
	uint64_t timestamp;
	struct ixev_ctx ctx;
	struct ip_tuple id;
	char *ptr;
	size_t bytes_left;
	binary_header_t header;
};

struct ixev_ctx client_ctx;

static struct client_conn *c;

static void client_die(void)
{
	ixev_close(&c->ctx);
	printf("remote connection was closed\n");
	exit(-1);
}

static int compare_uint64(const void *a, const void *b)
{
	const uint64_t *ua = (const uint64_t *) a;
	const uint64_t *ub = (const uint64_t *) b;

	return (*ua > *ub) - (*ua < *ub);
}

struct results {
	unsigned long min, avg, median, p95, p99, p999, max;
};

static struct results *calculate_results(void)
{
	struct results *r = malloc(sizeof(struct results));
	int i;
	uint64_t avg = 0;

	if (!r)
		return NULL;

	qsort(&samples, N, sizeof(uint64_t), &compare_uint64);
	for (i = 0; i < N; i++)
		avg += samples[i];

	avg /= N;

	r->min		= samples[0] / TSC_RATE;
	r->avg		= avg / TSC_RATE;
	r->median	= samples[N / 2] / TSC_RATE;
	r->p95		= samples[950 * N / 1000] / TSC_RATE;
	r->p99		= samples[990 * N / 1000] / TSC_RATE;
	r->p999		= samples[999 * N / 1000] / TSC_RATE;
	r->max		= samples[N - 1] / TSC_RATE;

	return r;
}

static void print_results(struct results *r)
{
	// min mean median 95th 99th 99.9th max
	printf("%ld %ld %ld %ld %ld %ld %ld\n",
	       r->min, r->avg, r->median, r->p95, r->p99, r->p999, r->max);
}

static void client_initial_setup(struct client_conn *c)
{
	const uint16_t keylen = 64;

	binary_header_t h = {0x80, CMD_GET, hton16(keylen),
			     0x00, 0x00, {hton16(0)}, hton32(keylen)};

	c->header = h;
	c->ptr = (char *) &c->header;
	c->bytes_left = HEADER_LEN + keylen;

	c->state = CLIENT_SEND;

	c->timestamp = rdtsc();
}

static void client_setup(struct client_conn *c)
{
	samples[sample_pos++] = rdtsc() - c->timestamp;

	if (sample_pos >= N) {
		struct results *r = calculate_results();
		print_results(r);
		/* send back the results to the mutilate coordinator */
		ixev_send(&c->ctx, r, sizeof(*r));
		
		return;
	}

	client_initial_setup(c);
}

static void client_send(struct client_conn *c)
{
	ssize_t ret;

	ret = ixev_send_zc(&c->ctx, c->ptr, c->bytes_left);
	if (ret <= 0) {
		if (ret != -EAGAIN)
			client_die();
		return;
	}

	c->bytes_left -= ret;
	if (c->bytes_left) {
		c->ptr += ret;
		return;
	}

	c->ptr = (char *) &c->header;
	c->bytes_left = HEADER_LEN;
	c->state = CLIENT_RECV_HDR;
}

static void client_recv_hdr(struct client_conn *c)
{
	ssize_t ret;

	ret = ixev_recv(&c->ctx, c->ptr, c->bytes_left);
	if (ret <= 0) {
		if (ret != -EAGAIN)
			client_die();
		return;
	}

	c->bytes_left -= ret;
	if (c->bytes_left) {
		c->ptr += ret;
		return;
	}

	c->ptr = c->header.buf;
	c->bytes_left = ntoh32(c->header.body_len);
	c->state = CLIENT_RECV_BODY;

	if (!c->bytes_left)
		client_setup(c);
}

static void client_recv_body(struct client_conn *c)
{
	ssize_t ret;

	ret = ixev_recv(&c->ctx, c->ptr, c->bytes_left);
	if (ret <= 0) {
		if (ret != -EAGAIN)
			client_die();
		return;
	}

	c->bytes_left -= ret;
	if (c->bytes_left) {
		c->ptr += ret;
		return;
	}

	client_setup(c);
}

static void main_handler(struct ixev_ctx *ctx, unsigned int reason)
{
	int last_state;

	do {
		last_state = c->state;

		//printf("state is %d\n", c->state);

		if (c->state == CLIENT_RECV_HDR) {
			client_recv_hdr(c);
		} else if (c->state == CLIENT_RECV_BODY) {
			client_recv_body(c);
		} else if (c->state == CLIENT_SEND) {
			client_send(c);
		}
	} while (c->state != last_state);

	if (c->state == CLIENT_SEND)
		ixev_set_handler(ctx, IXEVOUT, &main_handler);
	else
		ixev_set_handler(ctx, IXEVIN, &main_handler);
}

static void req_handler(struct ixev_ctx *ctx, unsigned int reason)
{
	char buf;

	ixev_recv(ctx, &buf, 1);

	if (sample_pos < N) {
		printf("spurious request\n");
		return;
	}

	sample_pos = 0;
	client_initial_setup(c);
	main_handler(&c->ctx, IXEVOUT);
}

static struct ixev_ctx *client_accept(struct ip_tuple *id)
{
	ixev_ctx_init(&client_ctx);
	ixev_set_handler(&client_ctx, IXEVIN, &req_handler);
	return &client_ctx;
}

static void client_release(struct ixev_ctx *ctx) { }

static void client_dialed(struct ixev_ctx *ctx, long ret)
{
	if (ret) {
		printf("failed to connect, ret = %ld\n", ret);
		exit(-1);
	}
}

struct ixev_conn_ops conn_ops = {
	.accept		= &client_accept,
	.release	= &client_release,
	.dialed		= &client_dialed,
};

static int parse_ip_addr(const char *str, uint32_t *addr)
{
	unsigned char a, b, c, d;

	if (sscanf(str, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) != 4)
		return -EINVAL;

	*addr = MAKE_IP_ADDR(a, b, c, d);
	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	c = malloc(sizeof(struct client_conn));
	if (!c)
		exit(-1);

	if (argc != 3) {
		fprintf(stderr, "Usage: IP PORT\n");
		return -1;
	}

	if (parse_ip_addr(argv[1], &c->id.dst_ip)) {
		fprintf(stderr, "Bad IP address '%s'", argv[1]);
		exit(1);
	}

	c->id.dst_port = atoi(argv[2]);

	printf("#min mean median 95th 99th 99.9th max\n");

	ixev_init(&conn_ops);

	ret = ixev_init_thread();
	if (ret) {
		printf("unable to init IXEV\n");
		exit(ret);
	};

	ixev_dial(&c->ctx, &c->id);

	while (1) {
		ixev_wait();
	}

	printf("exited\n");
	return 0;
}

