/*
 * init.c - initialization
 */

#include <pthread.h>
#include <unistd.h>

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
#include <ix/lock.h>
#include <ix/cfg.h>

#include <net/ip.h>

#ifdef ENABLE_PCAP
#include <net/pcap.h>
#endif

#include <dune.h>

#include <lwip/memp.h>


extern int net_init(void);
extern int tcp_api_init(void);
extern int ixgbe_init(struct pci_dev *pci_dev,
		      struct rte_eth_dev **ethp);
extern int sandbox_init(int argc, char *argv[]);
extern void tcp_init(void);
extern void toeplitz_init(void);
extern int cp_init(void);

volatile int uaccess_fault;

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
		ret = dune_vm_lookup(pgroot, (void *) addr,
				     CREATE_NORMAL, &pte);
		assert(!ret);
		*pte = PTE_P | PTE_W | PTE_ADDR(dune_va_to_pa((void *) addr));
	}
}

/**
 * init_create_ethdev - initializes an ethernet device
 * @pci_addr: the PCI address of the device
 *
 * FIXME: For now this is IXGBE-specific.
 *
 * Returns 0 if successful, otherwise fail.
 */
static int init_create_ethdev(const struct pci_addr *addr)
{
	struct pci_dev *dev;
	struct rte_eth_dev *eth;
	int ret;

	dev = pci_alloc_dev(addr);
	if (!dev)
		return -ENOMEM;

	ret = ixgbe_init(dev, &eth);
	if (ret) {
		log_err("init: failed to start driver\n");
		goto err;
	}

	ret = eth_dev_add(eth);
	if (ret) {
		log_err("init: unable to add ethernet device\n");
		goto err_add;
	}

	return 0;

err_add:
	eth_dev_destroy(eth);
err:
	free(dev);
	return ret;
}

static DEFINE_SPINLOCK(assign_lock);

static int network_init_cpu(void)
{
	int ret, idx, i;

	spin_lock(&assign_lock);
	for (i = 0; i < eth_dev_count; i++) {
		struct rte_eth_dev *eth = eth_dev[i];
		ret = eth_dev_get_rx_queue(eth, &percpu_get(eth_rxqs[i]));
		if (ret) {
			spin_unlock(&assign_lock);
			return ret;
		}

		ret = eth_dev_get_tx_queue(eth, &percpu_get(eth_txqs[i]));
		if (ret) {
			spin_unlock(&assign_lock);
			return ret;
		}
	}
	spin_unlock(&assign_lock);

	percpu_get(eth_num_queues) = eth_dev_count;

	ret = memp_init();
	if (ret) {
		log_err("init: failed to initialize lwip memp\n");
		return ret;
	}

	/* initialize perqueue data structures */
	for_each_queue(idx) {
		perqueue_get(eth_txq) = percpu_get(eth_txqs[idx]);
		perqueue_get(queue_id) = idx;
		tcp_init();
	}

	ret = tcp_api_init();
	if (ret) {
		log_err("init: failed to initialize TCP API\n");
		return ret;
	}

	unset_current_queue();

	return 0;
}

/**
 * init_create_cpu - initializes a CPU
 * @cpu: the CPU number
 * @eth: the ethernet device to assign to this CPU
 *
 * Returns 0 if successful, otherwise fail.
 */
static int init_create_cpu(unsigned int cpu)
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

	ret = network_init_cpu();
	if (ret) {
		log_err("init: unable to initialize per-CPU network stack\n");
		return ret;
	}

	log_info("init: CPU %d ready\n", cpu);

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

static pthread_barrier_t start_barrier;
static volatile int started_cpus;

void *start_cpu(void *arg)
{
	int ret;
	unsigned int cpu = *((unsigned int *) arg);

	ret = init_create_cpu(cpu);
	if (ret) {
		log_err("init: failed to initialize CPU %d\n", cpu);
		exit(ret);
	}

	started_cpus++;

	pthread_barrier_wait(&start_barrier);
	wait_for_spawn();

	return NULL;
}

static int init_hw(void)
{
	int i, ret;
	pthread_t tid;
	struct rte_eth_rss_reta rss_reta;
	int j, step;

	for (i = 0; i < cfg_dev_nr; i++) {
		ret = init_create_ethdev(&cfg_dev[i]);
		if (ret) {
			log_err("init: failed to initialize ethernet controller\n");
			return ret;
		}
	}

	cpus_active = cfg_cpu_nr;
	if (cfg_cpu_nr > 1) {
		pthread_barrier_init(&start_barrier, NULL, cfg_cpu_nr);
	}

	ret = init_create_cpu(cfg_cpu[0]);
	if (ret) {
		log_err("init: failed to initialize CPU 0\n");
		return ret;
	}

	for (i = 1; i < cfg_cpu_nr; i++) {
		ret = pthread_create(&tid, NULL, start_cpu, &cfg_cpu[i]);
		if (ret) {
			log_err("init: unable to create pthread\n");
			return -EAGAIN;
		}
		while (started_cpus != i)
			usleep(100);
	}

	if (cfg_cpu_nr > 1) {
		pthread_barrier_wait(&start_barrier);
	}

	rss_reta.mask_hi = -1;
	rss_reta.mask_lo = -1;
	step = cfg_dev_nr * ETH_RSS_RETA_NUM_ENTRIES / cfg_cpu_nr;
	if (cfg_dev_nr * ETH_RSS_RETA_NUM_ENTRIES % cfg_cpu_nr != 0)
		step++;

	for (i = 0; i < cfg_dev_nr; i++) {
		struct rte_eth_dev *eth = eth_dev[i];

		if (!eth->data->nb_rx_queues)
			continue;

		ret = eth_dev_start(eth);
		if (ret) {
			log_err("init: failed to start eth%d\n", i);
			return ret;
		}

		for (j = 0; j < ETH_RSS_RETA_NUM_ENTRIES; j++)
			rss_reta.reta[j] = (i * ETH_RSS_RETA_NUM_ENTRIES + j) / step;
		eth->dev_ops->reta_update(eth, &rss_reta);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret, args_parsed;

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

	dune_register_pgflt_handler(pgflt_handler);

	ret = timer_init();
	if (ret) {
		log_err("init: failed to initialize timers\n");
		return ret;
	}

	ret = net_init();
	if (ret) {
		log_err("init: failed to initialize netstack\n");
		return ret;
	}

	ret = cfg_init(argc, argv, &args_parsed);
	if (ret) {
		log_err("init: failed to load configuration\n");
		return ret;
	}

	toeplitz_init();

	ret = cp_init();
	if (ret) {
		log_err("init: failed to initialize control plane\n");
		return ret;
	}

	ret = init_hw();
	if (ret) {
		log_err("init: failed to initialize hardware\n");
		return ret;
	}

	ret = sandbox_init(argc - args_parsed - 1, &argv[args_parsed]);
	if (ret) {
		log_err("init: failed to start sandbox\n");
		return ret;
	}

	return 0;
}

