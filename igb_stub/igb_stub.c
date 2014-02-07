#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>

#define IXGBE_STATUS		0x00008
#define IXGBE_GPIE		0x00898
#define IXGBE_EIMS		0x00880
#define IXGBE_TCPTIMER		0x0004C
#define IXGBE_EICR		0x00800
#define IXGBE_EIAM		0x00890

#define IXGBE_TCPTIMER_KS		0x00000100
#define IXGBE_TCPTIMER_COUNT_ENABLE	0x00000200
#define IXGBE_TCPTIMER_COUNT_FINISH	0x00000400
#define IXGBE_TCPTIMER_LOOP		0x00000800
#define IXGBE_TCPTIMER_DURATION_MASK	0x000000FF

#define IXGBE_READ_REG(a, reg) readl((a) + (reg))
#define IXGBE_WRITE_REG(a, reg, value) writel((value), ((a) + (reg)))
#define IXGBE_WRITE_FLUSH(a) IXGBE_READ_REG(a, IXGBE_STATUS)

static struct pci_device_id igb_stub_pci_ids[] = {
	/* TODO: add more devices here */
	{ PCI_DEVICE(0x8086, 0x154d) },
	{ 0, },
};

static int interrupts_count;

static irqreturn_t intr_handler(int irq, void *dev_id)
{
	void *hw_addr = dev_id;
        u32 eicr;

        eicr = IXGBE_READ_REG(hw_addr, IXGBE_EICR);
        if (!eicr)
                return IRQ_NONE;

	interrupts_count++;
	IXGBE_WRITE_REG(hw_addr, IXGBE_EIMS, 1<<30);

	return IRQ_HANDLED;
}

static int igb_stub_set_interrupt_mask(struct pci_dev *pdev, int32_t state)
{
	uint32_t status;
	uint16_t old, new;

	pci_read_config_dword(pdev, PCI_COMMAND, &status);
	old = status;
	if (state != 0)
		new = old & (~PCI_COMMAND_INTX_DISABLE);
	else
		new = old | PCI_COMMAND_INTX_DISABLE;

	if (old != new)
		pci_write_config_word(pdev, PCI_COMMAND, new);

	return 0;
}

static int igb_stub_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int ret;
	void *hw_addr;

	/*
	 * enable device: ask low-level code to enable I/O and
	 * memory
	 */
	if (pci_enable_device(dev)) {
		printk(KERN_ERR "Cannot enable PCI device\n");
		goto fail;
	}

	/*
	 * reserve device's PCI memory regions for use by this
	 * module
	 */
	if (pci_request_regions(dev, "igb_stub")) {
		printk(KERN_ERR "Cannot request regions\n");
		goto fail_disable;
	}

	/* enable bus mastering on the device */
	pci_set_master(dev);

	hw_addr = ioremap(pci_resource_start(dev, 0), pci_resource_len(dev, 0));
	if (!hw_addr) {
		printk(KERN_ERR "Cannot ioremap\n");
		goto fail_release;
	}

	/* set 64-bit DMA mask */
	if (pci_set_dma_mask(dev,  DMA_BIT_MASK(64))) {
		printk(KERN_ERR "Cannot set DMA mask\n");
		goto fail_unmap;
	} else if (pci_set_consistent_dma_mask(dev, DMA_BIT_MASK(64))) {
		printk(KERN_ERR "Cannot set consistent DMA mask\n");
		goto fail_unmap;
	}

	ret = request_irq(dev->irq, intr_handler, IRQF_SHARED, "igb_stub", hw_addr);
	if (ret)
		goto fail_unmap;

	igb_stub_set_interrupt_mask(dev, 1);

	pci_set_drvdata(dev, hw_addr);

	//printk(KERN_INFO "GPIE = %x\n", IXGBE_READ_REG(hw_addr, IXGBE_GPIE));
	//printk(KERN_INFO "EIMS = %x\n", IXGBE_READ_REG(hw_addr, IXGBE_EIMS));
	IXGBE_WRITE_REG(hw_addr, IXGBE_EIMS, 1<<30);
	IXGBE_WRITE_REG(hw_addr, IXGBE_EIAM, 1<<30);
	IXGBE_WRITE_REG(hw_addr, IXGBE_TCPTIMER, 0xff | IXGBE_TCPTIMER_LOOP | IXGBE_TCPTIMER_COUNT_ENABLE | IXGBE_TCPTIMER_KS);
	IXGBE_WRITE_FLUSH(hw_addr);

	printk(KERN_INFO "ok\n");

	return 0;

fail_unmap:
	iounmap(hw_addr);
fail_release:
	pci_release_regions(dev);
fail_disable:
	pci_disable_device(dev);
fail:
	return -ENODEV;
}

static void igb_stub_pci_remove(struct pci_dev *dev)
{
	void *hw_addr = pci_get_drvdata(dev);
	free_irq(dev->irq, hw_addr);
	iounmap(hw_addr);
	pci_release_regions(dev);
	pci_disable_device(dev);
	pci_set_drvdata(dev, NULL);
}

static struct pci_driver igb_stub_pci_driver = {
	.name = "igb_stub",
	.id_table = igb_stub_pci_ids,
	.probe = igb_stub_pci_probe,
	.remove = igb_stub_pci_remove,
};

static int __init igb_stub_init(void)
{
	return pci_register_driver(&igb_stub_pci_driver);
}

static void __exit igb_stub_exit(void)
{
	printk(KERN_INFO "Interrupts count = %d\n", interrupts_count);
	pci_unregister_driver(&igb_stub_pci_driver);
}

module_init(igb_stub_init)
module_exit(igb_stub_exit)
