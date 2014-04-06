#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <ix.h>

#define BATCH_DEPTH	32

static void tcp_knock(hid_t handle, struct ip_tuple *id)
{
	ix_tcp_accept(handle, 0);
}

static void tcp_recv(hid_t handle, unsigned long cookie,
		     void *addr, size_t len)
{
	printf("got data of size %ld\n", len);
	ix_tcp_recv_done(handle, len);
}

static void tcp_dead(hid_t handle, unsigned long cookie)
{
	ix_tcp_close(handle);
}

static struct ix_ops ops = {
	.tcp_knock	= tcp_knock,
	.tcp_recv	= tcp_recv,
	.tcp_dead	= tcp_dead,
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

