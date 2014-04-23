/*
 * ixev.h - the IX high-level event library
 */

#include <ix/stddef.h>
#include <ix/mempool.h>
#include <stdio.h>
#include <errno.h>

#include "ixev.h"
#include "buf.h"

#define CMD_BATCH_SIZE	4096

/* FIXME: implement automatic TCP Buffer Tuning, Jeffrey Semke et. al. */
#define IXEV_SEND_WIN_SIZE	65536

static __thread struct bsys_ret ksys_ret_tbl[CMD_BATCH_SIZE];
static __thread uint64_t ixev_generation;
static struct ixev_conn_ops ixev_global_ops;

__thread struct mempool ixev_buf_pool;

static inline void __ixev_check_generation(struct ixev_ctx *ctx)
{
	if (ixev_generation != ctx->generation) {
		ctx->generation = ixev_generation;
		ctx->recv_done_desc = NULL;
		ctx->sendv_desc = NULL;
	}
}

static inline void
__ixev_recv_done(struct ixev_ctx *ctx, size_t len)
{
	__ixev_check_generation(ctx);

	if (!ctx->recv_done_desc) {
		ctx->recv_done_desc = __bsys_arr_next(karr);
		ixev_check_hacks(ctx);
		ksys_tcp_recv_done(ctx->recv_done_desc, ctx->handle, len);
	} else {
		ctx->recv_done_desc->argb += (uint64_t) len;
	}
}

static inline void
__ixev_sendv(struct ixev_ctx *ctx, struct sg_entry *ents, unsigned int nrents)
{
	__ixev_check_generation(ctx);

	if (!ctx->sendv_desc) {
		ctx->sendv_desc = __bsys_arr_next(karr);
		ixev_check_hacks(ctx);
		ksys_tcp_sendv(ctx->sendv_desc, ctx->handle, ents, nrents);
	} else {
		ctx->sendv_desc->argb = (uint64_t) ents;
		ctx->sendv_desc->argc = (uint64_t) nrents;
	}
}

static inline void
__ixev_close(struct ixev_ctx *ctx)
{
	struct bsys_desc *d = __bsys_arr_next(karr);
	ixev_check_hacks(ctx);
	ksys_tcp_close(d, ctx->handle);
}

static void ixev_tcp_knock(hid_t handle, struct ip_tuple *id)
{
	struct ixev_ctx *ctx = ixev_global_ops.accept(id);

	if (!ctx) {
		ix_tcp_reject(handle);
		return;
	}

	ctx->handle = handle;
	ix_tcp_accept(handle, (unsigned long) ctx);
}

static void ixev_tcp_dead(hid_t handle, unsigned long cookie)
{
	struct ixev_ctx *ctx = (struct ixev_ctx *) cookie;

	ctx->is_dead = true;
	if (ctx->en_mask & IXEVHUP)
		ctx->handler(ctx, IXEVHUP);
	else if (ctx->en_mask & IXEVIN)
		ctx->handler(ctx, IXEVIN | IXEVHUP);
	else
		ctx->trig_mask |= IXEVHUP;
}

static void ixev_tcp_recv(hid_t handle, unsigned long cookie,
			  void *addr, size_t len)
{
	struct ixev_ctx *ctx = (struct ixev_ctx *) cookie;
	uint16_t pos = ((ctx->recv_tail) & (IXEV_RECV_DEPTH - 1));
	struct sg_entry *ent;

	if (unlikely(ctx->recv_tail - ctx->recv_head + 1 >= IXEV_RECV_DEPTH)) {
		/*
		 * FIXME: We've run out of space for received packets. This
		 * will probably not happen unless the application is
		 * misbehaving and does not accept new packets for long
		 * periods of time or if a remote host sends a lot of too
		 * small packets.
		 *
		 * Possible solutions:
		 * 1.) Start copying into a buffer and accept up to the kernel
		 * receive window size in data.
		 * 2.) Allocate more descriptors and leave memory in mbufs, but
		 * this could waste memory.
		 */
		printf("ixev: ran out of receive memory\n");
		exit(-1);
	}

	ent = &ctx->recv[pos];
	ent->base = addr;
	ent->len = len;
	ctx->recv_tail++;

	if (ctx->en_mask & IXEVIN)
		ctx->handler(ctx, IXEVIN);
	else
		ctx->trig_mask |= IXEVIN;
}

static void ixev_tcp_sent(hid_t handle, unsigned long cookie, size_t len)
{
	struct ixev_ctx *ctx = (struct ixev_ctx *) cookie;

	if (ctx->en_mask & IXEVOUT)
		ctx->handler(ctx, IXEVOUT);
	else
		ctx->trig_mask |= IXEVOUT;
}

static struct ix_ops ixev_ops = {
	.tcp_knock	= ixev_tcp_knock,
	.tcp_dead	= ixev_tcp_dead,
	.tcp_recv	= ixev_tcp_recv,
	.tcp_sent	= ixev_tcp_sent,
};

/**
 * ixev_recv - read data with copying
 * @ctx: the context
 * @buf: a buffer to store the data
 * @len: the length to read
 *
 * Returns the number of bytes read, or 0 if there wasn't any data.
 */
ssize_t ixev_recv(struct ixev_ctx *ctx, void *buf, size_t len)
{
	size_t pos = 0;
	char *cbuf = (char *) buf;

	if (ctx->is_dead)
		return -EIO;

	while (ctx->recv_head != ctx->recv_tail) {
		struct sg_entry *ent =
			&ctx->recv[ctx->recv_head & (IXEV_RECV_DEPTH - 1)];
		size_t left = len - pos;

		if (!left)
			break;

		if (left >= ent->len) {
			memcpy(cbuf + pos, ent->base, ent->len);
			pos += ent->len;
			ctx->recv_head++;
		} else {
			memcpy(cbuf + pos, ent->base, left);
			ent->base = (char *) ent->base + left;
			ent->len -= left;
			pos += left;
			break;
		}
	}

	if (!pos)
		return -EAGAIN;

	__ixev_recv_done(ctx, pos);
	return pos;
}

static ssize_t
__ixev_send_zc(struct ixev_ctx *ctx, void *buf,
	       size_t actual_len, struct ixev_putcb *cb)
{
	struct sg_entry *ent;

	if (ctx->send_count >= IXEV_SEND_DEPTH)
		return -EAGAIN;

	ent = &ctx->send[ctx->send_count];
	ent->base = buf;
	ent->len = actual_len;

	if (cb)
		ctx->send_cb[ctx->send_count] = *cb;
	else
		ctx->send_cb[ctx->send_count].cb = NULL;

	ctx->send_count++;
	__ixev_sendv(ctx, ctx->send, ctx->send_count);

	return actual_len;
}

static void __ixev_send_release(void *arg)
{
	struct ixev_buf *buf = (struct ixev_buf *) arg;

	ixev_buf_release(buf);
}

/*
 * ixev_send - send data using copying
 * @ctx: the context
 * @addr: the address of the data
 * @len: the length of the data
 *
 * This variant is easier to use but is slower because it must first
 * copy the data to a buffer.
 *
 * Returns the number of bytes sent, or <0 if there was an error.
 */
ssize_t ixev_send(struct ixev_ctx *ctx, void *addr, size_t len)
{
	size_t actual_len = min(IXEV_SEND_WIN_SIZE - ctx->send_len, len);
	struct ixev_putcb *send_cb;
	struct sg_entry *ent;
	char *caddr = (char *) addr;
	ssize_t ret, so_far = 0;
	struct ixev_putcb cb;

	if (ctx->is_dead)
		return -EIO;

	cb.cb = &__ixev_send_release;

	if (!actual_len)
		return -EAGAIN;

	if (!ctx->send_count)
		goto cold_path;

	send_cb = &ctx->send_cb[ctx->send_count - 1];

	/* hot path: is there already a buffer? */
	if (send_cb->cb == &__ixev_send_release) {
		struct ixev_buf *buf = (struct ixev_buf *) send_cb->arg;

		ent = &ctx->send[ctx->send_count - 1];
		ret = ixev_buf_store(buf, caddr, actual_len);
		actual_len -= ret;
		ent->len += ret;
		caddr += ret;
		so_far += ret;
	}

cold_path:
	/* cold path: allocate and fill new buffers */
	while (actual_len) {
		struct ixev_buf *buf = ixev_buf_alloc();
		if (!buf) {
			if (so_far)
				goto out;
			return -EAGAIN;
		}

		ret = ixev_buf_store(buf, caddr, actual_len);

		cb.arg = (void *) buf;
		ret = __ixev_send_zc(ctx, buf->payload, ret, &cb);
		if (ret <= 0) {
			if (so_far)
				goto out;
			return ret;
		}

		actual_len -= ret;
		caddr += ret;
		so_far += ret;
	}

out:
	ctx->send_len += so_far;
	return so_far;
}

/*
 * ixev_send_zc - send data using zero-copy
 * @ctx: the context
 * @addr: the address of the data
 * @len: the length of the data
 * @cb: the completion callback when references to the data are dropped
 *
 * The callback could be NULL. Since TCP is a stream protocol, a possible
 * pattern is to batch up several requests then free all the memory
 * with a callback attached to the last request in the series.
 *
 * Returns the number of bytes sent, or <0 if there was an error.
 */
ssize_t ixev_send_zc(struct ixev_ctx *ctx, void *addr, size_t len,
		     struct ixev_putcb *cb)
{
	ssize_t ret;
	size_t actual_len = min(IXEV_SEND_WIN_SIZE - ctx->send_len, len);

	if (ctx->is_dead)
		return -EIO;
	if (!actual_len)
		return -EAGAIN;

	ret = __ixev_send_zc(ctx, addr, actual_len, cb);
	if (ret <= 0)
		return ret;

	ctx->send_len += actual_len;
	return actual_len;
}

/**
 * ixev_close - closes a context
 * @ctx: the context
 */
void ixev_close(struct ixev_ctx *ctx)
{
	ctx->en_mask = 0;
	__ixev_close(ctx);
}

/**
 * ixev_ctx_init - prepares a context for use
 * @ctx: the context
 * @ops: the event ops
 */
void ixev_ctx_init(struct ixev_ctx *ctx)
{
	ctx->en_mask = 0;
	ctx->trig_mask = 0;
	ctx->recv_head = 0;
	ctx->recv_tail = 0;
	ctx->send_count = 0;
	ctx->send_len = 0;
	ctx->recv_done_desc = NULL;
	ctx->sendv_desc = NULL;
	ctx->generation = 0;
	ctx->is_dead = false;
}

static void ixev_bad_ret(struct ixev_ctx *ctx, uint64_t sysnr, long ret)
{
	printf("ixev: unexpected failure return code %ld\n", ret);
	printf("ixev: sysnr %ld, ctx %p\n", sysnr, ctx);
	printf("ixev: error is fatal, terminating...\n");

	exit(-1);
}

static void ixev_shift_sends(struct ixev_ctx *ctx, int shift)
{
	int i;

	ctx->send_count -= shift;

	for (i = 0; i < ctx->send_count; i++) {
		ctx->send[i] = ctx->send[i + shift];
		ctx->send_cb[i] = ctx->send_cb[i + shift];
	}
}

static void ixev_handle_sendv_ret(struct ixev_ctx *ctx, long ret)
{
	int i;

	if (ret < 0) {
		ctx->is_dead = true;
		return;
	}

	ctx->send_len -= ret;

	for (i = 0; i < ctx->send_count; i++) {
		struct sg_entry *ent = &ctx->send[i];
		struct ixev_putcb *cb = &ctx->send_cb[i];
		if (ret < ent->len) {
			ent->len -= ret;
			ent->base = (char *) ent->base + ret;
			break;
		}

		if (cb->cb)
			cb->cb(cb->arg);
		ret -= ent->len;
	}

	ixev_shift_sends(ctx, i);
}

static void ixev_handle_close_ret(struct ixev_ctx *ctx, long ret)
{
	int i;

	if (unlikely(ret < 0)) {
		printf("ixev: failed to close handle, ret = %ld\n", ret);
		return;
	}

	for (i = 0; i < ctx->send_count; i++) {
		struct ixev_putcb *cb = &ctx->send_cb[i];
		if (cb->cb)
			cb->cb(cb->arg);
	}

	ixev_global_ops.release(ctx);
}

static void ixev_handle_one_ret(struct bsys_ret *r)
{
	struct ixev_ctx *ctx = (struct ixev_ctx *) r->cookie;
	uint64_t sysnr = r->sysnr;
	long ret = r->ret;

	switch (sysnr) {
	case KSYS_TCP_CONNECT:
		ctx->handle = ret;
		ixev_global_ops.connect_ret(ctx, ret);
		break;

	case KSYS_TCP_SENDV:
		ixev_handle_sendv_ret(ctx, ret);
		break;

	case KSYS_TCP_CLOSE:
		ixev_handle_close_ret(ctx, ret);
		break;

	default:
		if (unlikely(ret))
			ixev_bad_ret(ctx, sysnr, ret);
	}
}

/**
 * ixev_wait - wait for new events
 */
void ixev_wait(void)
{
	int i, nr_rets;

	/*
	 * FIXME: don't use the low-level library,
	 * just make system calls directly.
	 */

	ix_poll();
	ixev_generation++;

	nr_rets = karr->len;
	karr->len = 0;
	memcpy(ksys_ret_tbl, karr->descs, sizeof(struct bsys_ret) * nr_rets);
 
	for (i = 0; i < nr_rets; i++)
		ixev_handle_one_ret(&ksys_ret_tbl[i]);

	ix_handle_events();
}


/**
 * ixev_set_handler - sets the event handler and which events trigger it
 * @ctx: the context
 * @mask: the event types that trigger
 * @handler: the handler
 */
void ixev_set_handler(struct ixev_ctx *ctx, unsigned int mask,
		      ixev_handler_t handler)
{
	ctx->en_mask = mask;
	ctx->handler = handler;
}

/**
 * ixev_init_thread - thread-local initializer
 *
 * Call once per thread.
 *
 * Returns zero if successful, otherwise fail.
 */
int ixev_init_thread(void)
{
	int ret;

	ret = mempool_create(&ixev_buf_pool, 131072, sizeof(struct ixev_buf));
	if (ret)
		return ret;

	ret = ix_init(&ixev_ops, CMD_BATCH_SIZE);
	if (ret) {
		mempool_destroy(&ixev_buf_pool);
		return ret;
	}

	return 0;
}

/**
 * ixev_init - global initializer
 * @conn_ops: operations for establishing new connections
 *
 * Call once.
 *
 * Returns zero if successful, otherwise fail.
 */
int ixev_init(struct ixev_conn_ops *ops)
{
	/* FIXME: check if running inside IX */

	ixev_global_ops = *ops;
	return 0;
}

