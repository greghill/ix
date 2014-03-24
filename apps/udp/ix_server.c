#include <stddef.h>
#include <stdio.h>

#include <ix.h>

#define BATCH_DEPTH	32

struct ip_tuple ids[BATCH_DEPTH];

static void udp_recv(void *addr, size_t len, struct ip_tuple *src)
{
	struct ip_tuple *dst = &ids[ix_bsys_idx()];
	dst->src_ip = src->dst_ip;
	dst->dst_ip = src->src_ip;
	dst->src_port = src->dst_port;
	dst->dst_port = src->src_port;

	ix_udp_send(addr, len, dst, (unsigned long) addr);
}

static void udp_send_ret(unsigned long cookie, int64_t ret)
{
	ix_udp_recv_done((void *) cookie);
}

static struct ix_ops ops = {
	.udp_recv     = udp_recv,
	.udp_send_ret = udp_send_ret,
};

int main(int argc, char *argv[])
{
	int ret;

	ret = ix_init(&ops, BATCH_DEPTH);
	if (ret) {
		printf("unable to init IX\n");
		return ret;
	}

	while (1) {
		ix_poll();
	}

	return 0;
}

