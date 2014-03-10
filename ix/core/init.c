/*
 * init.c - main system and CPU core initialization
 */

#include <ix/stddef.h>
#include <ix/log.h>
#include <ix/errno.h>
#include <ix/pci.h>
#include <ix/ethdev.h>
#include <ix/timer.h>
#include <ix/cpu.h>
#include <ix/mbuf.h>

#include <dune.h>

/* FIXME: remove when we replace LWIP memory management with ours */
#include <lwip/mem.h>
#include <lwip/memp.h>
#include <lwip/pbuf.h>

extern int net_init(void);
extern int ixgbe_init(struct pci_dev *pci_dev, struct rte_eth_dev **ethp);
extern int virtual_init(void);
extern int read_configuration(const char *path);
extern int tcp_echo_server_init(int port);
extern int sandbox_init(int argc, char *argv[]);

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

static void main_loop(void)
{
	while (1) {
		timer_run();
		eth_tx_reclaim(eth_tx);
		eth_rx_poll(eth_rx);
	}
}

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

	timer_init_cpu();
	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	log_info("init: starting IX\n");

	if (argc < 2) {
		log_err("init: invalid arguments\n");
		log_err("init: format -> ix [ETH_PCI_ADDR]\n");
		return -EINVAL;
	}

	ret = read_configuration("ix.cfg");
	if (ret) {
		log_err("init: failed to read and parse configuration\n");
		return ret;
	}

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

	ret = init_this_cpu(1);
	if (ret) {
		log_err("init: failed to initialize the local CPU\n");
		return ret;
	}

	if (!strcmp(argv[1], "virtual")) {
		ret = virtual_init();
		if (ret) {
			log_err("init: failed to initialize ethernet device\n");
			return ret;
		}
	} else {
		ret = hw_init_one(argv[1]);
		if (ret) {
			log_err("init: failed to initialize ethernet device\n");
			return ret;
		}
	}

	ret = net_init();
	if (ret) {
		log_err("init: failed to initialize net\n");
		return ret;
	}

	/* FIXME: remove when we replace LWIP memory management with ours */
	mem_init();
	memp_init();
	pbuf_init();

	tcp_echo_server_init(1234);

#if 0
	ret = sandbox_init(argc - 2, &argv[2]);
	if (ret) {
		log_err("init: failed to start sandbox\n");
		return ret;
	}
#else
	main_loop();
#endif

	return 0;
}

