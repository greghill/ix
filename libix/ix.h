/*
 * ix.h - the main libix header
 */

#pragma once

#include "syscall.h"

struct ix_ops {
	void (*udp_recv)	(void *addr, size_t len, struct ip_tuple *id);
	void (*udp_send_ret)	(unsigned long cookie, int64_t ret);
	void (*tcp_knock)	(int handle, struct ip_tuple *id);
	void (*tcp_recv)	(int handle, unsigned long cookie,
				 void *addr, size_t len);
	void (*tcp_connect_ret)	(int handle, unsigned long cookie,
				 int ret);
	void (*tcp_send_ret)	(int handle, unsigned long cookie,
				 ssize_t ret);
	void (*tcp_dead)	(int handle, unsigned long cookie);
};

extern void ix_flush(void);
extern struct bsys_arr *karr;

static inline int ix_bsys_idx(void)
{
	return karr->len;
}

static inline void ix_udp_send(void *addr, size_t len, struct ip_tuple *id,
			       unsigned long cookie)
{
	if (karr->len >= karr->max_len)
		ix_flush();
	
	ksys_udp_send(__bsys_arr_next(karr), addr, len, id, cookie);
}

static inline void ix_udp_sendv(struct sg_entry *ents, unsigned int nrents,
				struct ip_tuple *id, unsigned long cookie)
{
	if (karr->len >= karr->max_len)
		ix_flush();
	
	ksys_udp_sendv(__bsys_arr_next(karr), ents, nrents, id, cookie);
}

static inline void ix_udp_recv_done(void *addr)
{
	if (karr->len >= karr->max_len)
		ix_flush();
	
	ksys_udp_recv_done(__bsys_arr_next(karr), addr);
}

static inline void ix_tcp_connect(struct ip_tuple *id, unsigned long cookie)
{
	if (karr->len >= karr->max_len)
		ix_flush();

	ksys_tcp_connect(__bsys_arr_next(karr), id, cookie);
}

static inline void ix_tcp_accept(int handle, unsigned long cookie)
{
	if (karr->len >= karr->max_len)
		ix_flush();

	ksys_tcp_accept(__bsys_arr_next(karr), handle, cookie);
}

static inline void ix_tcp_reject(int handle)
{
	if (karr->len >= karr->max_len)
		ix_flush();

	ksys_tcp_reject(__bsys_arr_next(karr), handle);
}

static inline void ix_tcp_send(int handle, void *addr, size_t len)
{
	if (karr->len >= karr->max_len)
		ix_flush();

	ksys_tcp_send(__bsys_arr_next(karr), handle, addr, len);
}

static inline void ix_tcp_sendv(int handle, struct sg_entry *ents,
				unsigned int nrents)
{
	if (karr->len >= karr->max_len)
		ix_flush();

	ksys_tcp_sendv(__bsys_arr_next(karr), handle, ents, nrents);
}

static inline void ix_tcp_recv_done(int handle, int nr)
{
	if (karr->len >= karr->max_len)
		ix_flush();

	ksys_tcp_recv_done(__bsys_arr_next(karr), handle, nr);
}

static inline void ix_tcp_close(int handle)
{
	if (karr->len >= karr->max_len)
		ix_flush();

	ksys_tcp_close(__bsys_arr_next(karr), handle);
}

extern int ix_poll(void);
extern int ix_init(struct ix_ops *ops, int batch_depth);

