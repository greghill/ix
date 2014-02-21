#include <linux/module.h>
#include <linux/pci.h>

static struct pci_device_id igb_stub_pci_ids[] = {
	/* TODO: add more devices here */
	{ PCI_DEVICE(0x8086, 0x154d) },
	{ 0, },
};

static int igb_stub_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
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

	return 0;

fail_disable:
	pci_disable_device(dev);
fail:
	return -ENODEV;
}

static void igb_stub_pci_remove(struct pci_dev *dev)
{
	pci_release_regions(dev);
	pci_disable_device(dev);
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
	pci_unregister_driver(&igb_stub_pci_driver);
}

module_init(igb_stub_init)
module_exit(igb_stub_exit)
