#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <ix.h>

#include <net/ip.h>

#define ECHO_PORT	10013
#define BATCH_DEPTH	32

static struct ip_tuple *id;
static int pipeline_cnt;

static void udp_recv(void *addr, size_t len, struct ip_tuple *src)
{
	if(src->dst_port == ECHO_PORT)
		pipeline_cnt--;

	ix_udp_recv_done(addr);
}

static void udp_send_ret(unsigned long cookie, int64_t ret)
{
	if (ret) {
		pipeline_cnt--;
		printf("packet %lx had error %ld\n", cookie, ret);
	}
}

static struct ix_ops ops = {
	.udp_recv     = udp_recv,
	.udp_send_ret = udp_send_ret,
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

	ret = ix_init(&ops, BATCH_DEPTH);
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

	while (1) {
		ix_poll();

		while (pipeline_cnt < 10) {
			ix_udp_send((void *) 0x100000000000, 64, id, 0);
			pipeline_cnt++;
		}

	}

	return 0;
}

