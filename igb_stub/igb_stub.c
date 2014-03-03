#include <linux/module.h>
#include <linux/pci.h>

/* Device IDs */
#define IXGBE_DEV_ID_82598			0x10B6
#define IXGBE_DEV_ID_82598_BX			0x1508
#define IXGBE_DEV_ID_82598AF_DUAL_PORT		0x10C6
#define IXGBE_DEV_ID_82598AF_SINGLE_PORT	0x10C7
#define IXGBE_DEV_ID_82598AT			0x10C8
#define IXGBE_DEV_ID_82598AT2			0x150B
#define IXGBE_DEV_ID_82598EB_SFP_LOM		0x10DB
#define IXGBE_DEV_ID_82598EB_CX4		0x10DD
#define IXGBE_DEV_ID_82598_CX4_DUAL_PORT	0x10EC
#define IXGBE_DEV_ID_82598_DA_DUAL_PORT		0x10F1
#define IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM	0x10E1
#define IXGBE_DEV_ID_82598EB_XF_LR		0x10F4
#define IXGBE_DEV_ID_82599_KX4			0x10F7
#define IXGBE_DEV_ID_82599_KX4_MEZZ		0x1514
#define IXGBE_DEV_ID_82599_KR			0x1517
#define IXGBE_DEV_ID_82599_COMBO_BACKPLANE	0x10F8
#define IXGBE_SUBDEV_ID_82599_KX4_KR_MEZZ	0x000C
#define IXGBE_DEV_ID_82599_CX4			0x10F9
#define IXGBE_DEV_ID_82599_SFP			0x10FB
#define IXGBE_SUBDEV_ID_82599_SFP		0x11A9
#define IXGBE_SUBDEV_ID_82599_RNDC		0x1F72
#define IXGBE_SUBDEV_ID_82599_560FLR		0x17D0
#define IXGBE_SUBDEV_ID_82599_ECNA_DP		0x0470
#define IXGBE_DEV_ID_82599_BACKPLANE_FCOE	0x152A
#define IXGBE_DEV_ID_82599_SFP_FCOE		0x1529
#define IXGBE_DEV_ID_82599_SFP_EM		0x1507
#define IXGBE_DEV_ID_82599_SFP_SF2		0x154D
#define IXGBE_DEV_ID_82599_SFP_SF_QP		0x154A
#define IXGBE_DEV_ID_82599_LS			0x154F
#define IXGBE_DEV_ID_82599EN_SFP		0x1557
#define IXGBE_DEV_ID_82599_QSFP_SF_QP		0x1558
#define IXGBE_DEV_ID_82599_XAUI_LOM		0x10FC
#define IXGBE_DEV_ID_82599_T3_LOM		0x151C
#define IXGBE_DEV_ID_82599_VF			0x10ED
#define IXGBE_DEV_ID_82599_VF_HV		0x152E
#define IXGBE_DEV_ID_X540T			0x1528
#define IXGBE_DEV_ID_X540_VF			0x1515
#define IXGBE_DEV_ID_X540_VF_HV			0x1530
#define IXGBE_DEV_ID_X540T1			0x1560

static struct pci_device_id igb_stub_pci_ids[] = {
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AF_DUAL_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AF_SINGLE_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598AT2)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_CX4)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_CX4_DUAL_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_DA_DUAL_PORT)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_XF_LR)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598EB_SFP_LOM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82598_BX)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KX4)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_XAUI_LOM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KR)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_EM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_KX4_MEZZ)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_CX4)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_BACKPLANE_FCOE)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_FCOE)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_T3_LOM)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_COMBO_BACKPLANE)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X540T)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_SF2)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_LS)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_QSFP_SF_QP)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599EN_SFP)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP_SF_QP)},
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_X540T1)},
	/* required last entry */
	{0, }
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
