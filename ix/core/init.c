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
//#include <ix/queue.h>
#include <ix/profiler.h>
#include <ix/lock.h>
#include <ix/cfg.h>
#include <ix/control_plane.h>
#include <ix/log.h>

#include <net/ip.h>

#ifdef ENABLE_PCAP
#include <net/pcap.h>
#endif

#include <dune.h>

#include <lwip/memp.h>


static int init_dune(void);
static int init_cfg(void);
static int init_firstcpu(void);
static int init_hw(void);
static int init_network_cpu(void);
static int init_ethdev(void);

extern int net_init(void);
extern int tcp_api_init(void);
extern int tcp_api_init_cpu(void);
extern int tcp_api_init_fg(void);
extern int ixgbe_init(struct pci_dev *pci_dev,
		      struct rte_eth_dev **ethp);
extern int sandbox_init(int argc, char *argv[]);
extern void tcp_init(void);
extern int toeplitz_init(void);
extern int cp_init(void);
extern int mempool_init(void);


struct init_vector_t {
	const char *name;
	int (*f)(void);
	int (*fcpu)(void);
	int (*ffg)(unsigned int);
};


static struct init_vector_t init_tbl[] =
{ { "CPU",     cpu_init,     NULL,NULL}, 
  { "Dune",    init_dune,    NULL,NULL},
  { "timer",   timer_init,   timer_init_cpu,NULL},
  { "net",     net_init,     NULL,NULL},
  { "cfg",     init_cfg,     NULL,NULL},               // after net
  { "toeplitz",toeplitz_init,NULL,NULL},
  { "cp",      cp_init,      NULL,NULL},
  { "firstcpu",init_firstcpu,NULL,NULL},                // after cfg
  { "mbuf",    mbuf_init,    mbuf_init_cpu,NULL},       // after firstcpu
  { "memp",    memp_init,    memp_init_cpu, NULL},
  { "tcpapi",  tcp_api_init, tcp_api_init_cpu,NULL},
  { "ethdev",  init_ethdev,  NULL, NULL},
  { "hw",      init_hw,      NULL,NULL},                // spaws per-cpu init sequence
  { "syscall", NULL,         syscall_init_cpu,NULL},
#ifdef ENABLE_KSTATS
  { "kstats",  NULL,         kstats_init_cpu, NULL},    // after timer
#endif
  { "init-net",NULL,         init_network_cpu,NULL},    // FIXME should be split
  { NULL,NULL,NULL,NULL}};


static int init_argc;
static char **init_argv;
static int args_parsed;

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
 * init_ethdev - initializes an ethernet device
 * @pci_addr: the PCI address of the device
 *
 * FIXME: For now this is IXGBE-specific.
 *
 * Returns 0 if successful, otherwise fail.
 */
static int init_ethdev(void)
{
	int ret;
	int i;
	for (i = 0; i < cfg_dev_nr; i++) {
                const struct pci_addr *addr = &cfg_dev[i];
		struct pci_dev *dev;
		struct rte_eth_dev *eth;
   
		dev = pci_alloc_dev(addr);
		if (!dev)
			return -ENOMEM;

		ret = ixgbe_init(dev, &eth);
		if (ret) {
			log_err("init: failed to start driver\n");
			free(dev);
			goto err;
		}

		ret = eth_dev_add(eth);
		if (ret) {
			log_err("init: unable to add ethernet device\n");
			eth_dev_destroy(eth);
			goto err;
		}
	}
	return 0;

err:
	return ret;
}

static DEFINE_SPINLOCK(assign_lock);

static int init_network_cpu(void)
{
	int ret, idx, i;
	spin_lock(&assign_lock);
	ret = 0;
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

	
#if 0	/* initialize perqueue data structures */
	for_each_queue(idx) {
		perqueue_get(eth_txq) = percpu_get(eth_txqs[idx]);
	}
#endif


	return 0;
}

/**
 * init_create_cpu - initializes a CPU
 * @cpu: the CPU number
 * @eth: the ethernet device to assign to this CPU
 *
 * Returns 0 if successful, otherwise fail.
 */
static int init_create_cpu(unsigned int cpu, int first)
{
	int ret=0,i;

	if (!first) 
		ret = cpu_init_one(cpu);

	if (ret) {
		log_err("init: unable to initialize CPU %d\n", cpu);
		return ret;
	}

	log_info("init: percpu phase %d\n",cpu);
	for (i=0;init_tbl[i].name;i++)
		if (init_tbl[i].fcpu) {
			ret = init_tbl[i].fcpu();
			log_info("init: module %-10s on %d: %s \n",init_tbl[i].name, percpu_get(cpu_id),(ret?"FAILURE":"SUCESS"));
			if (ret)
				panic("could not initialize IX\n");
		}


	log_info("init: CPU %d ready\n", cpu);
	printf("init:CPU %d ready\n",cpu);
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


	ret = init_create_cpu(cpu,0);
	if (ret) {
		log_err("init: failed to initialize CPU %d\n", cpu);
		exit(ret);
	}

	started_cpus++;

	percpu_get(cp_cmd) = &cp_shmem->command[started_cpus];

	pthread_barrier_wait(&start_barrier);
	wait_for_spawn();

	return NULL;
}

static int init_hw(void)
{
	int i, ret = 0;
	pthread_t tid;
	int j, step;
	int fg_id;
	DEFINE_BITMAP(fg_bitmap, ETH_MAX_TOTAL_FG);

	// will spawn per-cpu initialization sequence on CPU0
	ret = init_create_cpu(cfg_cpu[0], 1);
	if (ret) {
		log_err("init: failed to create CPU 0\n");
		return ret;
	}

	percpu_get(cp_cmd) = &cp_shmem->command[0];

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

	log_info("init: barrier after al CPU initialization\n");

	step = cfg_dev_nr * ETH_RSS_RETA_NUM_ENTRIES / cfg_cpu_nr;
	if (cfg_dev_nr * ETH_RSS_RETA_NUM_ENTRIES % cfg_cpu_nr != 0)
		step++;

	fg_id = 0;
	bitmap_init(fg_bitmap, ETH_MAX_TOTAL_FG, 0);
	for (i = 0; i < cfg_dev_nr; i++) {
		struct rte_eth_dev *eth = eth_dev[i];

		if (!eth->data->nb_rx_queues)
			continue;

		ret = eth_dev_start(eth);
		if (ret) {
			log_err("init: failed to start eth%d\n", i);
			return ret;
		}

		for (j = 0; j < eth->data->nb_rx_fgs; j++) {
			eth_fg_init_cpu(&eth->data->rx_fgs[j]);
			fgs[fg_id] = &eth->data->rx_fgs[j];

                        /* fake assignment, valid only during init */
			bitmap_set(fg_bitmap, fg_id);
			eth_fg_assign_to_cpu(fg_bitmap,0);
			eth_fg_set_current(&eth->data->rx_fgs[j]);

			perfg_get(fg_id) = fg_id;
			perfg_get(dev_idx) = i;

			assert(fgs[fg_id]->cur_cpu == percpu_get(cpu_id));

			tcp_init();
			ret = tcp_api_init_fg();
			if (ret) {
				log_err("init: failed to initialize tcp_api \n");
				return ret;
			}

			timer_init_fg();

			/* assign fg to various cpu */
			if (j == 0)
				eth_fg_assign_to_cpu(fg_bitmap, 0);
			else
				eth_fg_assign_to_cpu(fg_bitmap, (i * ETH_RSS_RETA_NUM_ENTRIES + j) / step);
			bitmap_clear(fg_bitmap, fg_id);


			fg_id++;
		}
	}

	unset_current_fg();

	nr_flow_groups = fg_id;
	cp_shmem->nr_flow_groups = nr_flow_groups;

	mempool_init();
	return 0;
}

static int init_dune(void)
{
	int ret = dune_init(false);
	if (ret) return ret;
	dune_register_pgflt_handler(pgflt_handler);
	return ret;
}

static int init_cfg(void)
{
	return cfg_init(init_argc,init_argv,&args_parsed);

}

static int init_firstcpu(void)
{
	int ret;
	cpus_active = cfg_cpu_nr;
	if (cfg_cpu_nr > 1) {
		pthread_barrier_init(&start_barrier, NULL, cfg_cpu_nr);
	}


	ret = cpu_init_one(cfg_cpu[0]);
	if (ret) {
		log_err("init: failed to initialize CPU 0\n");
		return ret;
	}
	return ret;
}

int main(int argc, char *argv[])
{
	int ret, i;

	init_argc = argc;
	init_argv = argv;

	log_info("init: starting IX\n");

	log_info("init: cpu phase\n");
	for (i=0;init_tbl[i].name;i++)
		if (init_tbl[i].f) {
			ret = init_tbl[i].f();
			log_info("init: module %-10s %s\n",init_tbl[i].name, (ret?"FAILURE":"SUCESS"));
			if (ret)
				panic("could not initialize IX\n");
		}

#ifdef ENABLE_PCAP
	if (cfg_pcap_mode == PCAP_MODE_WRITE) {
		ret = pcap_open_write(cfg_pcap_file);
		if (ret) {
			log_err("init: failed to open pcap file\n");
			return ret;
		}
		log_info("init: dumping traffic to pcap file '%s'\n", cfg_pcap_file);
	}
#endif

	ret = sandbox_init(argc - args_parsed - 1, &argv[args_parsed]);
	if (ret) {
		log_err("init: failed to start sandbox\n");
		return ret;
	}

	log_info("init done\n");
	return 0;
}

