/*
 * ix.h - the main libix header
 */

#pragma once

#include "syscall.h"

struct ix_ops {
	void (*udp_recv)	(void *addr, size_t len, struct ip_tuple *id);
	void (*udp_send_ret)	(unsigned long cookie, int64_t ret);
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

extern int ix_poll(void);
extern int ix_init(struct ix_ops *ops, int batch_depth);

