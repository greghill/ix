#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client_common.h"

void get_ifname(struct sockaddr_in *server_addr, char *ifname)
{
	struct ifaddrs *ifAddrStruct;
	struct ifaddrs *ifa;
	uint32_t netmask;
	uint32_t addr;

	getifaddrs(&ifAddrStruct);
	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		addr = ((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr;
		netmask = ((struct sockaddr_in *) ifa->ifa_netmask)->sin_addr.s_addr;
		if ((addr & netmask) != (server_addr->sin_addr.s_addr & netmask))
			continue;
		strcpy(ifname, ifa->ifa_name);
		break;
	}
	if (ifAddrStruct)
		freeifaddrs(ifAddrStruct);
}

void get_eth_stats(char *ifname, long *rx_bytes, long *rx_packets, long *tx_bytes, long *tx_packets)
{
	int i;
	FILE *fd;
	char buffer[1024];
	char *tok;

	fd = fopen("/proc/net/dev", "r");
	if (!fd) {
		perror("fopen");
		exit(1);
	}
	while (1) {
		if (!fgets(buffer, sizeof(buffer), fd))
			break;
		if (!strstr(buffer, ifname))
			continue;
		tok = strtok(buffer, " ");
		for (i = 0; i < 11 && tok; i++) {
			if (i == 1)
				*rx_bytes = strtol(tok, NULL, 10);
			else if (i == 2)
				*rx_packets = strtol(tok, NULL, 10);
			else if (i == 9)
				*tx_bytes = strtol(tok, NULL, 10);
			else if (i == 10)
				*tx_packets = strtol(tok, NULL, 10);
			tok = strtok(NULL, " ");
		}
	}
	fclose(fd);
}
