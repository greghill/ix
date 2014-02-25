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
		} else {
			log_err("cfg: unrecognized config keyword %s\n", buffer);
			return -EINVAL;
		}
	}

	return 0;
}
