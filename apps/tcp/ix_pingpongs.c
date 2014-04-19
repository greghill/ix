#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <ix.h>
#include <ix/hash.h>
#include <ix/list.h>

#define BATCH_DEPTH	512

#define BUFFER_POOL_2MB_PAGES	1
#define BUFFER_POOL_SIZE	(BUFFER_POOL_2MB_PAGES * PGSIZE_2MB)
#define CTX_MAX_ENTRIES		65536
#define CTX_HASH_SEED		0xa36bdcbe

static __thread struct sg_entry *ents;

static size_t msg_size;

struct buffer_pool_entry {
	struct hlist_node link;
	char *pointer;
};

struct ctx {
	hid_t handle;
	struct hlist_node link;
	size_t bytes_left;
	struct buffer_pool_entry *buffer;
};

static __thread struct hlist_head ctx_tbl[CTX_MAX_ENTRIES];
static __thread char *buffer_pool;
static __thread struct hlist_head buffer_pool_head;

static inline int handle_to_idx(hid_t handle)
{
	int idx = hash_crc32c_one(CTX_HASH_SEED, handle);
	idx &= CTX_MAX_ENTRIES - 1;
	return idx;
}

static inline struct ctx *ctx_get(hid_t handle)
{
	struct ctx *ctx;
	struct hlist_node *pos;
	struct hlist_head *h = &ctx_tbl[handle_to_idx(handle)];

	hlist_for_each(h, pos) {
		ctx = hlist_entry(pos, struct ctx, link);
		if (ctx->handle == handle)
			return ctx;
	}

	return NULL;
}

static inline void ctx_put(hid_t handle, struct ctx *ctx)
{
	struct hlist_head *h;

	ctx->handle = handle;
	h = &ctx_tbl[handle_to_idx(handle)];
	hlist_add_head(h, &ctx->link);
}

static inline void ctx_del(struct ctx *ctx)
{
	if (ctx)
		hlist_del(&ctx->link);
}

static void tcp_knock(hid_t handle, struct ip_tuple *id)
{
	struct ctx *ctx;
	struct buffer_pool_entry *entry;

	entry = hlist_entry(buffer_pool_head.head, struct buffer_pool_entry, link);
	if (!entry)
		return;
	hlist_del_head(&buffer_pool_head);

	ctx = malloc(sizeof(*ctx));
	ctx->bytes_left = msg_size;
	ctx->buffer = entry;

	ctx_put(handle, ctx);

	ix_tcp_accept(handle, 0);
}

static void tcp_recv(hid_t handle, unsigned long cookie,
		     void *addr, size_t len)
{
	struct ctx *ctx;

	ctx = ctx_get(handle);
	if (!ctx)
		goto out;

	memcpy(&ctx->buffer->pointer[msg_size - ctx->bytes_left], addr, len);
	ctx->bytes_left -= len;
	if (!ctx->bytes_left) {
		struct sg_entry *ent = &ents[ix_bsys_idx()];

		ent->base = ctx->buffer->pointer;
		ent->len = msg_size;

		ctx->bytes_left = msg_size;

		/*
		 * FIXME: this will work fine except if the send window
		 * becomes full. Better code is in development still.
		 */
		ix_tcp_sendv(handle, ent, 1);
	}
out:
	ix_tcp_recv_done(handle, len);
}

static void tcp_dead(hid_t handle, unsigned long cookie)
{
	struct ctx *ctx;

	ctx = ctx_get(handle);
	if (!ctx)
		goto out;

	hlist_add_head(&buffer_pool_head, &ctx->buffer->link);
	free(ctx);
	ctx_del(ctx);

out:
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
	.tcp_sent	= tcp_sent,
};

static void *thread_main(void *arg)
{
	int ret;
	int cpu, tmp;
	int i;
	struct buffer_pool_entry *entry;

	ret = ix_init(&ops, BATCH_DEPTH);
	if (ret) {
		printf("unable to init IX\n");
		return NULL;
	}

	ents = malloc(sizeof(struct sg_entry) * BATCH_DEPTH);
	if (!ents)
		return NULL;

	ret = syscall(SYS_getcpu, &cpu, &tmp, NULL);
	if (ret) {
		printf("unable to get current CPU\n");
		return NULL;
	}

	buffer_pool = (char *) MEM_USER_IOMAPM_BASE_ADDR + cpu * BUFFER_POOL_SIZE;
	ret = sys_mmap(buffer_pool, BUFFER_POOL_2MB_PAGES, PGSIZE_2MB, VM_PERM_R | VM_PERM_W);
	if (ret) {
		printf("unable to allocate memory for zero-copy\n");
		return NULL;
	}

	for (i = 0; i <= BUFFER_POOL_SIZE - msg_size; i += align_up(msg_size, 64)) {
		entry = malloc(sizeof(*entry));
		if (!entry) {
			printf("unable to allocate memory for buffer pool management\n");
			return NULL;
		}
		entry->pointer = &buffer_pool[i];
		hlist_add_head(&buffer_pool_head, &entry->link);
	}

	while (1) {
		ix_poll();
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int nr_cpu, i;
	pthread_t tid;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s MSG_SIZE\n", argv[0]);
		return 1;
	}

	msg_size = atoi(argv[1]);
	if (msg_size > BUFFER_POOL_SIZE) {
		fprintf(stderr, "Error: MSG_SIZE (%ld) is larger than maximum allowed (%d).\n", msg_size, BUFFER_POOL_SIZE);
		return 1;
	}

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
