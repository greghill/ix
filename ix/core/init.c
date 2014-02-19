/*
 * init.c - main system and CPU core initialization
 */

#include <ix/stddef.h>
#include <ix/log.h>
#include <ix/errno.h>
#include <ix/pci.h>
#include <ix/ethdev.h>

extern int timer_init(void);
extern int net_init(void);
extern int ixgbe_init(struct pci_dev *pci_dev, struct rte_eth_dev **ethp);

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
		free(dev);
		return ret;
	}

	eth_dev_start(eth);

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

	ret = timer_init();
	if (ret) {
		log_err("init: failed to initialize timers\n");
		return ret;
	}

	ret = mbuf_init_core();
	if (ret) {
		log_err("init: unable to initialize mbufs\n");
		return ret;
	}

	ret = hw_init_one(argv[1]);
	if (ret) {
		log_err("init: failed to initialize ethernet device\n");
		return ret;
	}

//	ret = net_init();
	if (ret) {
		log_err("init: failed to initialize net\n");
		return ret;
	}

	return 0;
}

