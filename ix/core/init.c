/*
 * init.c - main system and CPU core initialization
 */

#define SANDBOX_ENABLED 1

#include <getopt.h>
#include <pthread.h>

#include <ix/stddef.h>
#include <ix/log.h>
#include <ix/errno.h>
#include <ix/pci.h>
#include <ix/ethdev.h>
#include <ix/timer.h>
#include <ix/cpu.h>
#include <ix/mbuf.h>
#include <ix/syscall.h>
#include <ix/kstats.h>
#include <ix/queue.h>
#include <ix/profiler.h>

#include <net/ip.h>
#include <net/icmp.h>

#ifdef ENABLE_PCAP
#include <net/pcap.h>
#endif

#include <dune.h>

#include <lwip/memp.h>

#define MAX_QUEUES 256

static int max_queues;
static int max_queues_per_core;

struct cpu_spec {
	int count;
	int cpu[MAX_QUEUES];
	int nb_rx_queues[MAX_QUEUES];
};

struct cpu_thread_params {
	int cpu;
	int nb_rx_queues;
	pthread_mutex_t start_init_mutex;
	pthread_mutex_t init_done_mutex;
};

static struct {
	char *dev;
	char *cpu;
	char *mac;
#ifdef ENABLE_PCAP
	char *pcap_file;
	enum {
		PCAP_MODE_NONE,
		PCAP_MODE_READ,
		PCAP_MODE_WRITE,
	} pcap_mode;
#endif
	char **restv;
	int restc;
} arguments;

extern int net_init(void);
extern int tcp_api_init(void);
extern int ixgbe_init(struct pci_dev *pci_dev, struct rte_eth_dev **ethp);
extern int virtual_init(void);
extern int tcp_echo_server_init(int port);
extern int sandbox_init(int argc, char *argv[]);
extern void tcp_init(void);
extern void toeplitz_init(void);
extern int cp_init(void);

volatile int uaccess_fault;

static int hw_init_one(const char *pci_addr)
{
	struct pci_addr addr;
	struct pci_dev *dev;
	struct rte_eth_dev *eth;
	int ret;

	ret = pci_str_to_addr(pci_addr, &addr);
	if (ret) {
		log_err("init: invalid PCI address string\n");
		return ret;
	}

	dev = pci_alloc_dev(&addr);
	if (!dev)
		return -ENOMEM;

	ret = ixgbe_init(dev, &eth);
	if (ret) {
		log_err("init: failed to start driver\n");
		goto err;
	}

	ret = eth_dev_start(eth);
	if (ret) {
		log_err("init: unable to start ethernet device\n");
		goto err_start;
	}

	return 0;

err_start:
	eth_dev_destroy(eth);
err:
	free(dev);
	return ret;
}

#if !SANDBOX_ENABLED
static void main_loop(void)
{
	unsigned int i, pkts, tx_len;

	while (1) {
		KSTATS_PUSH(timer, NULL);
		timer_run();
		KSTATS_POP(NULL);

		KSTATS_PUSH(tx_reclaim, NULL);
		percpu_get(tx_batch_cap) = eth_tx_reclaim(percpu_get(eth_tx));
		KSTATS_POP(NULL);

		KSTATS_PUSH(rx_poll, NULL);
		pkts = 0;
		for_each_queue(i)
			pkts += eth_rx_poll(percpu_get(eth_rx)[i]);
		KSTATS_POP(NULL);

		KSTATS_PUSH(tx_xmit, NULL);
		tx_len = eth_tx_xmit(percpu_get(eth_tx),
				     percpu_get(tx_batch_pos),
				     percpu_get(tx_batch));
		if (tx_len != percpu_get(tx_batch_pos))
			panic("transmit failed, expected %d, got %d\n",
			      percpu_get(tx_batch_pos), tx_len);
		percpu_get(tx_batch_len) = 0;
		percpu_get(tx_batch_pos) = 0;
		KSTATS_POP(NULL);

		if (!pkts) {
			KSTATS_PUSH(idle, NULL);
			eth_rx_idle_wait(10 * ONE_MS);
			KSTATS_POP(NULL);
		}
	}
}
#endif

static void
pgflt_handler(uintptr_t addr, uint64_t fec, struct dune_tf *tf)
{
	int ret;
	ptent_t *pte;
	bool was_user = (tf->cs & 0x3);

	if (was_user) {
		printf("sandbox: got unexpected G3 page fault"
		       " at addr %lx, fec %lx\n", addr, fec);
		dune_dump_trap_frame(tf);
		dune_ret_from_user(-EFAULT);
	} else {
		ret = dune_vm_lookup(pgroot, (void *) addr, CREATE_NORMAL, &pte);
		assert(!ret);
		*pte = PTE_P | PTE_W | PTE_ADDR(dune_va_to_pa((void *) addr));
	}
}

static int init_this_cpu(unsigned int cpu)
{
	int ret;

	ret = cpu_init_one(cpu);
	if (ret) {
		log_err("init: unable to initialize CPU %d\n", cpu);
		return ret;
	}

	ret = mbuf_init_cpu();
	if (ret) {
		log_err("init: unable to initialize mbufs\n");
		return ret;
	}

	ret = syscall_init_cpu();
	if (ret) {
		log_err("init: unable to initialize batched syscall array\n");
		return ret;
	}

	timer_init_cpu();
	kstats_init_cpu();

	return 0;
}

#if !SANDBOX_ENABLED
static int parse_ip_addr(const char *string, uint32_t *addr)
{
        unsigned char a, b, c, d;

        if (sscanf(string, "%hhu.%hhu.%hhu.%hhu", &a, &b, &c, &d) != 4)
                return -EINVAL;

        *addr = MAKE_IP_ADDR(a, b, c, d);

        return 0;
}
#endif

#ifdef ENABLE_PCAP
static int parse_eth_addr(const char *string, struct eth_addr *mac)
{
	struct eth_addr tmp;

	if (sscanf(string, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
	           &tmp.addr[0], &tmp.addr[1], &tmp.addr[2],
		   &tmp.addr[3], &tmp.addr[4], &tmp.addr[5]) != 6)
		return -EINVAL;

	*mac = tmp;

	return 0;
}
#endif

#if !SANDBOX_ENABLED
static void main_loop_ping(struct ip_addr *dst, uint16_t id, uint16_t seq)
{
	unsigned int i;
	uint64_t last_ping = 0;
	uint64_t now;

	while (1) {
		timer_run();
		eth_tx_reclaim(percpu_get(eth_tx));
		for_each_queue(i)
			eth_rx_poll(percpu_get(eth_rx)[i]);

		now = rdtsc();
		if (now - last_ping >= 1000000ull * cycles_per_us) {
			icmp_echo(dst, id, seq++, now);
			last_ping = now;
		}
	}
}
#endif

static int get_rx_queue(struct eth_rx_queue **rx_queue)
{
	static int eth_dev_idx = 0;

	if (eth_dev_idx >= eth_dev_count)
		eth_dev_idx = 0;
	return eth_dev_get_rx_queue(eth_dev[eth_dev_idx++], rx_queue);
}

static int get_tx_queue(struct eth_tx_queue **tx_queue)
{
	static int eth_dev_idx = 0;

	if (eth_dev_idx >= eth_dev_count)
		eth_dev_idx = 0;
	return eth_dev_get_tx_queue(eth_dev[eth_dev_idx++], tx_queue);
}

static int cpu_networking_init(int nb_rx_queues)
{
	int ret;
	unsigned int queue;
	int i;

	if (nb_rx_queues > max_queues_per_core)
		return -EINVAL;

	percpu_get(eth_rx_count) = 0;
	percpu_get(eth_rx) = malloc(sizeof(struct eth_rx_queue *) * max_queues_per_core);
	if (!percpu_get(eth_rx)) {
		log_err("init: failed to allocate memory for RX queues\n");
		return -ENOMEM;
	}

	percpu_get(eth_rx_bitmap) = 0;
	for (i = 0; i < nb_rx_queues; i++) {
		ret = get_rx_queue(&percpu_get(eth_rx)[i]);
		percpu_get(eth_rx_bitmap) |= 1 << percpu_get(eth_rx)[i]->queue_idx;
		if (ret) {
			log_err("init: failed to get an RX queue\n");
			return ret;
		}

		set_current_queue(percpu_get(eth_rx)[i]);
		perqueue_get(queue_id) = i;
	}
	percpu_get(eth_rx_count) = i;

	ret = get_tx_queue(&percpu_get(eth_tx));
	if (ret) {
		log_err("init: failed to get a TX queue\n");
		return ret;
	}

	ret = memp_init();
	if (ret) {
		log_err("init: failed to initialize lwip memp\n");
		return ret;
	}

	for_each_queue(queue)
		tcp_init();

#if SANDBOX_ENABLED
	ret = tcp_api_init();
	if (ret) {
		log_err("init: failed to initialize TCP API\n");
		return ret;
	}
#else
	tcp_echo_server_init(1234);
#endif

	log_info("init: cpu %d handles %d queues\n", percpu_get(cpu_id), percpu_get(eth_rx_count));

	return 0;
}

static pthread_mutex_t spawn_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t spawn_cond = PTHREAD_COND_INITIALIZER;

struct spawn_req {
	void *arg;
	struct spawn_req *next;
};

static struct spawn_req *spawn_reqs;
extern void *pthread_entry(void *arg);

static void wait_for_spawn(void)
{
	struct spawn_req *req;
	void *arg;

	pthread_mutex_lock(&spawn_mutex);
	while (!spawn_reqs)
		pthread_cond_wait(&spawn_cond, &spawn_mutex);
	req = spawn_reqs;
	spawn_reqs = spawn_reqs->next;
	pthread_mutex_unlock(&spawn_mutex);

	arg = req->arg;
	free(req);

	log_info("init: user spawned cpu %d\n", percpu_get(cpu_id));
	pthread_entry(arg);
}

int init_do_spawn(void *arg)
{
	struct spawn_req *req;

	pthread_mutex_lock(&spawn_mutex);
	req = malloc(sizeof(struct spawn_req));
	if (!req) {
		pthread_mutex_unlock(&spawn_mutex);
		return -ENOMEM;
	}

	req->next = spawn_reqs;
	req->arg = arg;
	spawn_reqs = req;
	pthread_cond_broadcast(&spawn_cond);
	pthread_mutex_unlock(&spawn_mutex);

	return 0;
}

void *start_cpu(void *arg)
{
	int ret;
	struct cpu_thread_params *params;

	params = arg;

	pthread_mutex_lock(&params->start_init_mutex);

	ret = init_this_cpu(params->cpu);
	if (ret) {
		log_err("init: failed to initialize CPU %d\n", params->cpu);
		exit(ret);
	}

	ret = cpu_networking_init(params->nb_rx_queues);
	if (ret) {
		log_err("init: failed to initialize networking for cpu\n");
		exit(ret);
	}

	pthread_mutex_unlock(&params->init_done_mutex);

#if SANDBOX_ENABLED
	wait_for_spawn();
#else
	main_loop();
#endif

	return NULL;
}

int parse_cpu_list(char *list, struct cpu_spec *cpu_spec)
{
	int i;
	int val;
	char *tok;
	char *tok2;
	char *saveptr1, *saveptr2;
	int total_rx_queues;
	int max_cpus;

	max_cpus = sizeof(cpu_spec->cpu) / sizeof(cpu_spec->cpu[0]);
	total_rx_queues = 0;
	cpu_spec->count = 0;
	tok = strtok_r(list, ",", &saveptr1);
	while (tok) {
		if (cpu_spec->count >= max_cpus) {
			log_err("init: a maximum number of %d cpus are supported.\n", max_cpus);
			return 1;
		}
		tok2 = strtok_r(tok, ":", &saveptr2);
		val = atoi(tok2);
		if (val < 0 || val >= cpu_count) {
			log_err("init: invalid cpu specified '%s'\n", tok2);
			return 1;
		}
		for (i = 0; i < cpu_spec->count; i++) {
			if (cpu_spec->cpu[i] == val) {
				log_err("init: cpu specified multiple times '%d'\n", val);
				return 1;
			}
		}
		cpu_spec->cpu[cpu_spec->count] = val;

		tok2 = strtok_r(NULL, ":", &saveptr2);
		if (tok2) {
			val = atoi(tok2);
			if (val < 0 || val > max_queues) {
				log_err("init: invalid rx queue count specified '%s'\n", tok2);
				return 1;
			}
			total_rx_queues += val;
			if (total_rx_queues > max_queues) {
				log_err("init: total rx queues cannot exceed %d\n", max_queues);
				return 1;
			}
		} else {
			val = -1;
		}
		cpu_spec->nb_rx_queues[cpu_spec->count] = val;

		cpu_spec->count++;
		tok = strtok_r(NULL, ",", &saveptr1);
	}

	cpus_active = cpu_spec->count;

	return 0;
}

void balance_queues_to_cores(struct cpu_spec *cpu_spec)
{
	int i;
	int count;
	int total_rx_queues;
	int count_unspec_queues;

	total_rx_queues = 0;
	count_unspec_queues = 0;
	for (i = 0; i < cpu_spec->count; i++)
		if (cpu_spec->nb_rx_queues[i] != -1)
			total_rx_queues += cpu_spec->nb_rx_queues[i];
		else
			count_unspec_queues++;

	if (!count_unspec_queues)
		return;

	count = (max_queues - total_rx_queues) % count_unspec_queues;
	for (i = 0; i < cpu_spec->count; i++) {
		if (cpu_spec->nb_rx_queues[i] != -1)
			continue;
		cpu_spec->nb_rx_queues[i] = (max_queues - total_rx_queues) / count_unspec_queues;
		if (count) {
			cpu_spec->nb_rx_queues[i]++;
			count--;
		}
	}
}

static int parse_arguments(int argc, char *argv[])
{
	int c;
	int required_dev;

	static struct option long_options[] = {
		{"dev", required_argument, 0, 'd'},
		{"cpu", required_argument, 0, 'c'},
		{"mac", required_argument, 0, 'm'},
		{"quiet", no_argument, 0, 'q'},
#ifdef ENABLE_PCAP
		{"pcap-read", required_argument, 0, 'r'},
		{"pcap-write", required_argument, 0, 'w'},
#endif
		{NULL, 0, NULL, 0}
	};
#ifdef ENABLE_PCAP
	static const char *optstring = "d:c:m:qr:w:";
#else
	static const char *optstring = "d:c:m:q";
#endif

	while ((c = getopt_long(argc, argv, optstring, long_options, NULL)) != -1) {
		switch (c) {
		case 'd':
			arguments.dev = optarg;
			break;
		case 'c':
			arguments.cpu = optarg;
			break;
		case 'm':
			arguments.mac = optarg;
			break;
		case 'q':
			max_loglevel = LOG_WARN;
			break;
#ifdef ENABLE_PCAP
		case 'r':
			arguments.pcap_file = optarg;
			arguments.pcap_mode = PCAP_MODE_READ;
			break;
		case 'w':
			arguments.pcap_file = optarg;
			arguments.pcap_mode = PCAP_MODE_WRITE;
			break;
#endif
		default:
			return 1;
		}
	}

	required_dev = 1;

#ifdef ENABLE_PCAP
	if (arguments.pcap_mode == PCAP_MODE_READ) {
		required_dev = 0;
		if (arguments.dev || arguments.cpu) {
			log_err("init: you cannot specify device or cpu for --pcap-read.\n");
			return 1;
		}
		if (!arguments.mac) {
			log_err("init: you must specify MAC address for --pcap-read.\n");
			return 1;
		}
	}
#endif

	if (required_dev && !arguments.dev) {
		log_err("init: you must specify device.\n");
		return 1;
	}

	arguments.restv = &argv[optind];
	arguments.restc = argc - optind - 1;

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	pthread_t thread;
	struct cpu_spec cpu_spec;
	int i;
	struct cpu_thread_params cpu_thread_params[MAX_QUEUES];
	char *eth_pci_addr;

	if (parse_arguments(argc, argv)) {
		log_err("init: invalid arguments\n");
		return -EINVAL;
	}

	log_info("init: starting IX\n");

	ret = cpu_init();
	if (ret) {
		log_err("init: failed to initalize CPU cores\n");
		return ret;
	}

	ret = dune_init(false);
	if (ret) {
		log_err("init: failed to initialize dune\n");
		return ret;
	}

	toeplitz_init();

	dune_register_pgflt_handler(pgflt_handler);

	ret = timer_init();
	if (ret) {
		log_err("init: failed to initialize timers\n");
		return ret;
	}

	if (arguments.dev && !strcmp(arguments.dev, "virtual")) {
		ret = virtual_init();
		if (ret) {
			log_err("init: failed to initialize ethernet device\n");
			return ret;
		}
#ifdef ENABLE_PCAP
	} else if (arguments.pcap_mode == PCAP_MODE_READ) {
		struct eth_addr mac;

		ret = parse_eth_addr(arguments.mac, &mac);
		if (ret) {
			log_err("init: failed to parse MAC address '%s'\n", arguments.mac);
			return ret;
		}

		ret = init_this_cpu(1);
		if (ret) {
			log_err("init: failed to initialize the local CPU\n");
			return ret;
		}

		ret = pcap_open_read(arguments.pcap_file, &mac);
		if (ret) {
			log_err("init: failed to open pcap file '%s'\n", arguments.pcap_file);
			return ret;
		}
#endif
	} else {
		eth_pci_addr = strtok(arguments.dev, ",");
		while (eth_pci_addr) {
			ret = hw_init_one(eth_pci_addr);
			if (ret) {
				log_err("init: failed to initialize ethernet device\n");
				return ret;
			}
			/* FIXME: query the device for the number of RX queues supported */
			max_queues += ETH_RSS_RETA_MAX_QUEUE;
			max_queues_per_core += ETH_RSS_RETA_MAX_QUEUE;
			eth_pci_addr = strtok(NULL, ",");
		}

		if (max_queues > MAX_QUEUES) {
			log_err("init: unsupported number of RX queues '%d'\n", max_queues);
			return -EINVAL;
		}

		if (arguments.cpu) {
			ret = parse_cpu_list(arguments.cpu, &cpu_spec);
			if (ret)
				return ret;
		} else {
			cpu_spec.count = 1;
			cpu_spec.cpu[0] = 1;
			cpu_spec.nb_rx_queues[0] = -1;
		}

		balance_queues_to_cores(&cpu_spec);

		for (i = 1; i < cpu_spec.count; i++) {
			cpu_thread_params[i].cpu = cpu_spec.cpu[i];
			cpu_thread_params[i].nb_rx_queues = cpu_spec.nb_rx_queues[i];
			pthread_mutex_init(&cpu_thread_params[i].start_init_mutex, NULL);
			pthread_mutex_init(&cpu_thread_params[i].init_done_mutex, NULL);
			pthread_mutex_lock(&cpu_thread_params[i].start_init_mutex);
			pthread_mutex_lock(&cpu_thread_params[i].init_done_mutex);
			ret = pthread_create(&thread, NULL, start_cpu, &cpu_thread_params[i]);
			if (ret) {
				log_err("init: failed to spawn thread\n");
				return ret;
			}
		}

		ret = init_this_cpu(cpu_spec.cpu[0]);
		if (ret) {
			log_err("init: failed to initialize the local CPU\n");
			return ret;
		}

		ret = cpu_networking_init(cpu_spec.nb_rx_queues[0]);
		if (ret) {
			log_err("init: failed to initialize networking for cpu\n");
			return ret;
		}

		profiler_init();
	}

	ret = net_init();
	if (ret) {
		log_err("init: failed to initialize net\n");
		return ret;
	}

	ret = cp_init();
	if (ret) {
		log_err("init: failed to initialize control plane\n");
		return ret;
	}

	for (i = 1; i < cpu_spec.count; i++) {
		pthread_mutex_unlock(&cpu_thread_params[i].start_init_mutex);
		pthread_mutex_lock(&cpu_thread_params[i].init_done_mutex);
		pthread_mutex_unlock(&cpu_thread_params[i].init_done_mutex);
		pthread_mutex_destroy(&cpu_thread_params[i].start_init_mutex);
		pthread_mutex_destroy(&cpu_thread_params[i].init_done_mutex);
	}

#ifdef ENABLE_PCAP
	if (arguments.pcap_mode == PCAP_MODE_READ) {
		log_info("init: entering pcap replay mode\n");
		pcap_replay();
		return 0;
	} else if (arguments.pcap_mode == PCAP_MODE_WRITE) {
		ret = pcap_open_write(arguments.pcap_file);
		if (ret) {
			log_err("init: failed to open pcap file\n");
			return ret;
		}
		log_info("init: dumping traffic to pcap file '%s'\n", arguments.pcap_file);
	}
#endif

#if SANDBOX_ENABLED
	ret = sandbox_init(arguments.restc, arguments.restv);
	if (ret) {
		log_err("init: failed to start sandbox\n");
		return ret;
	}
#else
	if (arguments.restc >= 2 && strcmp(arguments.restv[0], "ping") == 0) {
		struct ip_addr dst;

		ret = parse_ip_addr(arguments.restv[1], &dst.addr);
		if (ret) {
			log_err("init: ping: invalid destination IP address\n");
			return ret;
		}

		log_info("init: ping: starting...\n");
		main_loop_ping(&dst, 0, 0);
	}

	main_loop();
#endif

	return 0;
}

