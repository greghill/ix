/*
 * cfg.c - configuration parameters
 *
 * FIXME: Probably the design we really want is to be able to have many
 * options in both the configuration file and the command line arguments,
 * giving override priority to command line arguments. Right now we have
 * some options in the file and some on the command line.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <ix/errno.h>
#include <ix/log.h>
#include <ix/types.h>
#include <ix/cfg.h>

#include <net/ethernet.h>
#include <net/ip.h>

struct ip_addr cfg_host_addr;
struct ip_addr cfg_broadcast_addr;
struct ip_addr cfg_gateway_addr;
uint32_t cfg_mask;
struct eth_addr cfg_mac;

unsigned int cfg_cpu[NCPU];
int cfg_cpu_nr;

struct pci_addr cfg_dev[NETHDEV];
int cfg_dev_nr;

extern int net_cfg(void);
extern int arp_insert(struct ip_addr *addr, struct eth_addr *mac);

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

static int parse_conf_file(const char *path)
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
				printf("cfg: failed to insert static ARP entry.\n");
		} else {
			printf("cfg: unrecognized config keyword %s\n", buffer);
			return -EINVAL;
		}
	}

	return 0;
}

static int insert_dev(struct pci_addr addr)
{
	int i;

	for (i = 0; i < cfg_dev_nr; i++) {
		if (!memcmp(&cfg_dev[i], &addr, sizeof(struct pci_addr))) {
			return 0;
		}
	}

	if (cfg_dev_nr >= NETHDEV)
		return -E2BIG;

	cfg_dev[cfg_dev_nr++] = addr;
	return 0;
}

static int parse_dev_list(char *list)
{
	int ret;
	struct pci_addr addr;
	char *pos = strtok(list, ",");

	while (pos) {
		ret = pci_str_to_addr(pos, &addr);
		if (ret) {
			printf("cfg: invalid PCI device string\n");
			return ret;
		}

		ret = insert_dev(addr);
		if (ret) {
			printf("cfg: too many PCI devices\n");
			return ret;
		}

		pos = strtok(NULL, ",");
	}

	return 0;
}

static int insert_cpu(unsigned int cpu)
{
	int i;

	for (i = 0; i < cfg_cpu_nr; i++) {
		if (cfg_cpu[i] == cpu) {
			return 0;
		}
	}

	if (cfg_cpu_nr >= NCPU)
		return -E2BIG;

	cfg_cpu[cfg_cpu_nr++] = cpu;
	return 0;
}

static int parse_cpu_list(char *list)
{
	int ret;
	unsigned int cpu;
	char *pos = strtok(list, ",");

	while (pos) {
		cpu = atoi(pos);

		if (cpu >= cpu_count) {
			printf("cfg: CPU %d is too large (only %d CPUs)\n",
			       cpu, cpu_count);
			return -EINVAL;
		}
		ret = insert_cpu(cpu);
		if (ret) {
			printf("cfg: too many CPUs\n");
			return ret;
		}

		pos = strtok(NULL, ",");
	}

	return 0;
}

static void print_usage(void)
{
	printf("ix [option]...\n"
	"\n"
	"Options\n"
	"--dev\n"
	"-d\n"
	"\tSpecifies the PCI device(s) to scan for ethernet controllers.\n"
	"\tFormat is a list dddd:bb:ss.ff,... d - domain, b = bus, s = slot\n"
	"\tf = function.\n"
	"--cpu\n"
	"-c\n"
	"\tIndicates which CPU(s) this IX instance should manage. CPUs\n"
	"\tare provided as a list of numbers (.e.g. -c 1,4,5).\n"
	"--quiet\n"
	"-q\n"
	"\tLimits logging to critical errors.\n");
}

static int parse_arguments(int argc, char *argv[], int *args_parsed)
{
	int c, ret;

	static struct option long_options[] = {
		{"dev", required_argument, NULL, 'd'},
		{"cpu", required_argument, NULL, 'c'},
		{"quiet", no_argument, NULL, 'q'},
		{NULL, 0, NULL, 0}
	};

	static const char *optstring = "d:c:q";

	/* FIXME: get packet capture working again */

	while (true) {
		c = getopt_long(argc, argv, optstring, long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'd':
			ret = parse_dev_list(optarg);
			if (ret)
				goto fail;
			break;
		case 'c':
			ret = parse_cpu_list(optarg);
			if (ret)
				goto fail;
			break;
		case 'q':
			max_loglevel = LOG_WARN;
			break;
		default:
			printf("cfg: invalid command option %x\n", c);
			ret = -EINVAL;
			goto fail;
		}
	}

	if (!cfg_cpu_nr) {
		printf("cfg: at least one CPU must be specified\n");
		ret = -EINVAL;
		goto fail;
	}

	if (!cfg_dev_nr) {
		printf("cfg: at least one PCI device must be specified\n");
		ret = -EINVAL;
		goto fail;
	}

	*args_parsed = optind;
	return 0;

fail:
	print_usage();
	return ret;
}

/**
 * cfg_init - parses configuration arguments and files
 * @argc: the number of arguments
 * @argv: the argument vector
 * @args_parsed: a pointer to store the number of arguments parsed
 *
 * Returns 0 if successful, otherwise fail.
 */
int cfg_init(int argc, char *argv[], int *args_parsed)
{
	int ret;

	ret = parse_arguments(argc, argv, args_parsed);
	if (ret)
		return ret;

	ret = parse_conf_file("ix.cfg");
	if (ret)
		return ret;

	ret = net_cfg();
	if (ret)
		return ret;

	return 0;
}

