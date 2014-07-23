/*
 * net.c - the main file for the network subsystem
 */

#include <ix/stddef.h>
#include <ix/log.h>
#include <ix/cfg.h>

#include "net.h"

static void net_dump_cfg(void)
{
	char str[IP_ADDR_STR_LEN];
	struct ip_addr mask = {cfg_mask};

	log_info("net: using the following configuration:\n");

	ip_addr_to_str(&cfg_host_addr, str);
	log_info("\thost IP:\t%s\n", str);
	ip_addr_to_str(&cfg_broadcast_addr, str);
	log_info("\tbroadcast IP:\t%s\n", str);
	ip_addr_to_str(&cfg_gateway_addr, str);
	log_info("\tgateway IP:\t%s\n", str);
	ip_addr_to_str(&mask, str);
	log_info("\tsubnet mask:\t%s\n", str);
}

/**
 * net_init - initializes the network stack
 *
 * Returns 0 if successful, otherwise fail.
 */
int net_init(void)
{
	int ret;

	ret = arp_init();
	if (ret) {
		log_err("net: failed to initialize arp\n");
		return ret;
	}

	return 0;
}

/**
 * net_cfg - load the network configuration parameters
 *
 * Returns 0 if successful, otherwise fail.
 */
int net_cfg(void)
{
	net_dump_cfg();

	return 0;
}

