/*
 * ethflowgrp.h - Support for flow groups, the basic unit of load balancing
 */

#pragma once

#include <ix/stddef.h>
#include <ix/list.h>
#include <ix/lock.h>

#define ETH_MAX_NUM_FG	128 

struct eth_fg {
	bool		in_transition;
	unsigned int	cur_cpu;
	unsigned int	target_cpu;
	unsigned int	idx;

	spinlock_t	lock;

	void (*steer) (struct eth_rx_queue *target);
};


struct eth_fg_listener {
	void (*prepare)	(struct eth_fg *fg);
	void (*finish)	(struct eth_fg *fg);
	struct list_node n;
};

extern void eth_fg_register_listener(struct eth_fg_listener *l);
extern void eth_fg_unregister_listener(struct eth_fg_listener *l);

