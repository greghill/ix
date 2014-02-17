#include <getopt.h>

#include <asm/cpu.h>
#include <ix/dyncore.h>
#include <net/ethernet.h>
#include <net/ip.h>

#include "cfg.h"
#include "ipv4.h"
#include "nic.h"

extern int timer_init(void);
extern int arp_init(void);
extern int dpdk_init(int argc, char *argv[]);

extern struct nic_operations dpdk_nic_ops;

struct nic_operations *nic_ops = &dpdk_nic_ops;

static int receive_pkts(__attribute__((unused)) void *dummy)
{
	struct rte_mbuf *pkts_burst[1];
	int nb_rx;

	cpu_serialize();
	nb_rx = nic_ops->receive_one_pkt(pkts_burst);

	if (nb_rx) {
		ipv4_rx_pkts(nb_rx, pkts_burst);
	}
	return 0;
}

static int has_pending_pkts(__attribute__((unused)) void *dummy)
{
	return nic_ops->has_pending_pkts();
}

void parse_arguments(int argc, char *argv[])
{
	int c;

	static struct option long_options[] = {
		{"ip", required_argument, 0, 'i'},
		{"mac", required_argument, 0, 'm'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "i:m:", long_options, NULL)) != -1) {
		switch (c) {
		case 'i':
		{
			unsigned char a, b, c, d;
			if (sscanf(optarg, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) != 4) {
				fprintf(stderr, "Malformed IP address.\n");
				exit(1);
			}
			cfg_host_addr.addr = MAKE_IP_ADDR(a, b, c, d);
			break;
		}
		case 'm':
			if (sscanf(optarg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &cfg_mac.addr[0], &cfg_mac.addr[1], &cfg_mac.addr[2], &cfg_mac.addr[3], &cfg_mac.addr[4], &cfg_mac.addr[5]) != 6) {
				fprintf(stderr, "Malformed MAC address.\n");
				exit(1);
			}
			break;
		default:
			exit(1);
		}
	}
}

int main(int argc, char *argv[])
{
	static char *dpdk_argv[] = {"", "-c1", "-n1"};

	parse_arguments(argc, argv);

	timer_init();
	arp_init();

	optind = 1;
	dpdk_init(3, dpdk_argv);
	nic_ops->init();
	dyncore_init(receive_pkts, has_pending_pkts);

	return 0;
}
