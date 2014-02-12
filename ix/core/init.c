/*
 * init.c - main system and CPU core initialization
 */

#include <ix/stddef.h>
#include <ix/log.h>

extern int timer_init(void);
extern int net_init(void);
extern int ixgbe_init(struct pci_dev *pci_dev, struct rte_eth_dev *ethp);

int main(int argc, char *argv[])
{
	int ret;

	log_info("init: starting IX\n");

	ret = timer_init();
	if (ret) {
		log_err("init: failed to initialize timers\n");
		return ret;
	}

//	ret = net_init();
	if (ret) {
		log_err("init: failed to initialize net\n");
		return ret;
	}

	return 0;
}

