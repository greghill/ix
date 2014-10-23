/*
 * cfg.h - configuration parameters
 */

#pragma once

#include <ix/cpu.h>
#include <ix/pci.h>
#include <ix/ethdev.h>

struct ip_addr;
struct eth_addr;

extern struct ip_addr cfg_host_addr, cfg_broadcast_addr, cfg_gateway_addr;
extern uint32_t cfg_mask;
extern struct eth_addr cfg_mac;

extern unsigned int cfg_cpu[NCPU];
extern int cfg_cpu_nr;

extern struct pci_addr cfg_dev[NETHDEV];
extern int cfg_dev_nr;

extern int cfg_init(int argc, char *argv[], int *args_parsed);

#ifdef ENABLE_PCAP
extern char *cfg_pcap_file;
enum pcap_mode {
	PCAP_MODE_NONE,
	PCAP_MODE_READ,
	PCAP_MODE_WRITE,
};
extern enum pcap_mode cfg_pcap_mode;
#endif
