#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <ix.h>

#define BATCH_DEPTH	512

static __thread struct sg_entry *ents;

static void tcp_knock(hid_t handle, struct ip_tuple *id)
{
	ix_tcp_accept(handle, 0);
}

static void tcp_recv(hid_t handle, unsigned long cookie,
		     void *addr, size_t len)
{
	struct sg_entry *ent = &ents[ix_bsys_idx()];

	ents[0].base = addr;
	ents[0].len = len;

	/*
	 * FIXME: this will work fine except if the send window
         * becomes full. Better code is in development still.
	 */
	ix_tcp_sendv(handle, ent, 1);
	ix_tcp_recv_done(handle, len);
}

static void tcp_dead(hid_t handle, unsigned long cookie)
{
	ix_tcp_close(handle);
}

static void
tcp_send_ret(hid_t handle, unsigned long cookie, ssize_t ret)
{

}

static void
tcp_sent(hid_t handle, unsigned long cookie, size_t win_size)
{

}

static struct ix_ops ops = {
	.tcp_knock	= tcp_knock,
	.tcp_recv	= tcp_recv,
	.tcp_dead	= tcp_dead,
	.tcp_send_ret	= tcp_send_ret,
	.tcp_xmit_win	= tcp_sent,
};

static void *thread_main(void *arg)
{
	int ret;

	ret = ix_init(&ops, BATCH_DEPTH);
	if (ret) {
		printf("unable to init IX\n");
		return NULL;
	}

	ents = malloc(sizeof(struct sg_entry) * BATCH_DEPTH);
	if (!ents)
		return NULL;

	while (1) {
		ix_poll();
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int nr_cpu, i;
	pthread_t tid;

	nr_cpu = sys_nrcpus();
	if (nr_cpu < 1) {
		printf("got invalid cpu count %d\n", nr_cpu);
		exit(-1);
	}
	nr_cpu--; /* don't count the main thread */

	sys_spawnmode(true);

	for (i = 0; i < nr_cpu; i++) {
		if (pthread_create(&tid, NULL, thread_main, NULL)) {
			printf("failed to spawn thread %d\n", i);
			exit(-1);
		}
	}

	thread_main(NULL);
	printf("exited\n");
	return 0;
}

