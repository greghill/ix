/*
 * cfg.c - network stack configuration parameters
 *
 * FIXME: we're hardcoded to a single gateway.
 *
 * FIXME: we're hardcoded to a single interface.
 */

#include <stdio.h>
#include <string.h>

#include <ix/errno.h>
#include <ix/log.h>
#include <ix/types.h>
#include <net/ip.h>
#include <net/ethernet.h>

#include "net.h"

struct ip_addr cfg_host_addr;
struct ip_addr cfg_broadcast_addr;
struct ip_addr cfg_gateway_addr;
uint32_t cfg_mask;
struct eth_addr cfg_mac;

static int parse_ip_addr(FILE *fd, uint32_t *addr)
{
	unsigned char a, b, c, d;

	if (fscanf(fd, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) != 4)
		return -EINVAL;

	*addr = MAKE_IP_ADDR(a, b, c, d);

	return 0;
}

static int parse_eth_addr(FILE *fd, struct eth_addr *mac)
{
	struct eth_addr tmp;

	if (fscanf(fd, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
	           &tmp.addr[0], &tmp.addr[1], &tmp.addr[2],
		   &tmp.addr[3], &tmp.addr[4], &tmp.addr[5]) != 6)
		return -EINVAL;

	*mac = tmp;

	return 0;
}

int read_configuration(const char *path)
{
	FILE *fd;
	int ret;
	char buffer[32];

	fd = fopen(path, "r");
	if (!fd) {
		return -EINVAL;
	}

	while (1) {
		ret = fscanf(fd, "%31s", buffer);
		if (ret == EOF)
			break;
		else if (ret != 1)
			return -EINVAL;

		if (!strcmp(buffer, "host_addr")) {
			if (parse_ip_addr(fd, &cfg_host_addr.addr))
				return -EINVAL;
		} else if (!strcmp(buffer, "broadcast_addr")) {
			if (parse_ip_addr(fd, &cfg_broadcast_addr.addr))
				return -EINVAL;
		} else if (!strcmp(buffer, "gateway_addr")) {
			if (parse_ip_addr(fd, &cfg_gateway_addr.addr))
				return -EINVAL;
		} else if (!strcmp(buffer, "mask")) {
			if (parse_ip_addr(fd, &cfg_mask))
				return -EINVAL;
		} else if (!strcmp(buffer, "arp")) {
			struct ip_addr addr;
			struct eth_addr mac;

			if (parse_ip_addr(fd, &addr.addr))
				return -EINVAL;

			if (parse_eth_addr(fd, &mac))
				return -EINVAL;

			if (arp_insert(&addr, &mac))
				log_warn("cfg: failed to insert static ARP entry.\n");
		} else {
			log_err("cfg: unrecognized config keyword %s\n", buffer);
			return -EINVAL;
		}
	}

	return 0;
}
