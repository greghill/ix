#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <ix.h>

#include <net/ip.h>

#define ECHO_PORT	10013
#define BATCH_DEPTH	256

#define TSC_RATE	3100000000
#define DROP_TICKS	TSC_RATE

static struct ip_tuple *id;
static size_t req_len;
static unsigned long pkt_cnt;
static unsigned long drop_cnt;

struct xmit_ctx {
	unsigned int idx;
	unsigned long magic;
	unsigned long tsc;
};

static char *xmit_buf;

static inline struct xmit_ctx *get_ctx(int i)
{
	return (struct xmit_ctx *) &xmit_buf[i * req_len];
}

static void udp_recv(void *addr, size_t len, struct ip_tuple *src)
{
	struct xmit_ctx *recv, *ctx;

	if(src->dst_port != ECHO_PORT)
		goto out;

	recv = (struct xmit_ctx *) addr;
	if (recv->idx >= BATCH_DEPTH) {
		printf("response had invalid context index %d\n", recv->idx);
		goto out;
	}

	ctx = get_ctx(recv->idx);
	if (recv->magic != ctx->magic) {
		printf("response had invalid magic value %lx (should be %lx)\n",
		       recv->magic, ctx->magic);
		goto out;
	}

	pkt_cnt++;

	ctx->magic++;
	ctx->tsc = rdtsc();

	ix_udp_send((void *) ctx, req_len, id, ctx->idx);

out:
	ix_udp_recv_done(addr);
}

static void udp_send_ret(unsigned long cookie, int64_t ret)
{
	if (ret) {
		struct xmit_ctx *ctx = get_ctx((int) cookie);

		if (ret != (int64_t) -RET_AGAIN)
			printf("packet %lx had error %ld\n", cookie, ret);

		ctx->tsc = rdtsc();
		ix_udp_send((void *) ctx, req_len, id, ctx->idx);
	}
}

static struct ix_ops ops = {
	.udp_recv     = udp_recv,
	.udp_send_ret = udp_send_ret,
};

static void kick_ctx(int i)
{
	struct xmit_ctx *ctx = get_ctx((int) i);

	ctx->idx = i;
	ctx->magic = 0;
	ctx->tsc = rdtsc();

	ix_udp_send((void *) ctx, req_len, id, i); 
}

static void check_drops(unsigned long tsc)
{
	int i;
	struct xmit_ctx * ctx;

	for (i = 0; i < BATCH_DEPTH; i++) {
		ctx = get_ctx((int) i);
		if (tsc - ctx->tsc >= DROP_TICKS) {
			drop_cnt++;
			ctx->tsc = tsc;
			ix_udp_send((void *) ctx, req_len, id, i);
		} 
	}
}

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
	int ret, i;
	unsigned long tsc;

	ret = ix_init(&ops, 1024);
	if (ret) {
		printf("unable to init IX\n");
		return ret;
	}

	id = malloc(sizeof(struct ip_tuple));
	if (!id)
		return -ENOMEM;

	ret = parse_ip_addr(argv[1], &id->dst_ip);
	if (ret) {
		printf("invalid arguments\n");
		return -EINVAL;
	}

	id->dst_port = ECHO_PORT;
	id->src_port = ECHO_PORT;

	xmit_buf = (char *) MEM_USER_IOMAPM_BASE_ADDR;
	ret = sys_mmap(xmit_buf, 1, PGSIZE_2MB, VM_PERM_R | VM_PERM_W);
	if (ret) {
		printf("unable to allocate memory for zero-copy\n");
		return ret;
	}

	req_len = 64;

	/* start sending requests */
	for (i = 0; i < BATCH_DEPTH; i++)
		kick_ctx(i);

	tsc = rdtsc();

	while (1) {
		unsigned long now;

		ix_poll();
		now = rdtsc();
		check_drops(now);

		if (now - tsc > TSC_RATE) {
			if (drop_cnt) {
				printf("%ld pkts / sec (%ld drops)\n", pkt_cnt, drop_cnt);
				drop_cnt = 0;
			} else
				printf("%ld pkts / sec\n", pkt_cnt);
			pkt_cnt = 0;
			tsc = now;
		}

	}

	return 0;
}

