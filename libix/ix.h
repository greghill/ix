/*
 * ix.h - the main libix header
 */

#pragma once

#include "syscall.h"

struct ix_ops {
	void (*udp_recv)	(void *addr, size_t len, struct ip_tuple *id);
	void (*udp_send_done)	(uint64_t count);
};

extern int ix_init(struct ix_ops *ops);

