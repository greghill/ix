/*
 * ix.h - the main libix header
 */

#pragma once

#include "syscall.h"

struct ix_ops {
	void (*udp_recv)	(void *addr, size_t len, struct ip_tuple *id);
	void (*udp_send_ret)	(int64_t ret);
};

extern void ix_flush(void);
extern struct bsys_arr *karr;

static inline void ix_udp_send(void *addr, size_t len, struct ip_tuple *id)
{
	if (karr->len >= karr->max_len)
		ix_flush();
	
	ksys_udp_send(__bsys_arr_next(karr), addr, len, id);
}

static inline void ix_udp_sendv(struct sg_entry *ents[], unsigned int nrents,
				struct ip_tuple *id)
{
	if (karr->len >= karr->max_len)
		ix_flush();
	
	ksys_udp_sendv(__bsys_arr_next(karr), ents, nrents, id);
}

static inline void ix_udp_recv_done(uint64_t count)
{
	if (karr->len >= karr->max_len)
		ix_flush();
	
	ksys_udp_recv_done(__bsys_arr_next(karr), count);
}

extern void ix_poll(void);
extern int ix_init(struct ix_ops *ops);

