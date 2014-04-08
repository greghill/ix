/*
 * net.c - the main file for the network subsystem
 */

#include <ix/stddef.h>
#include <ix/log.h>

#include "net.h"
#include "cfg.h"

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

int net_init(void)
{
	int ret;
	int i;

	log_info("net: starting networking\n");

	ret = arp_init();
	if (ret) {
		log_err("net: failed to initialize arp\n");
		return ret;
	}

	ret = read_configuration("ix.cfg");
	if (ret) {
		log_err("init: failed to read and parse configuration\n");
		return ret;
	}

	net_dump_cfg();

	eth_dev_get_hw_mac(eth_dev[0], &cfg_mac);

	for (i = 1; i < eth_dev_count; i++)
		eth_dev_set_hw_mac(eth_dev[i], &cfg_mac);

	return 0;
}

