#include <asm/cpu.h>
#include <ix/dyncore.h>

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
	unsigned long tsc;

	cpu_serialize();
	tsc = rdtsc();
	nb_rx = nic_ops->receive_one_pkt(pkts_burst);

	if (nb_rx) {
		ipv4_rx_pkts(nb_rx, pkts_burst);
		printf("%d packets took %ld cycles\n", nb_rx, rdtscp(NULL) - tsc);
	}
	return 0;
}

static int has_pending_pkts(__attribute__((unused)) void *dummy)
{
	return nic_ops->has_pending_pkts();
}

#if 0
int main(int argc, char *argv[])
{
	timer_init();
	arp_init();
	dpdk_init(argc, argv);
	dyncore_init(receive_pkts, has_pending_pkts);

	return 0;
}
#endif
