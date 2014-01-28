/*
 * pci.h - PCI bus support
 */

#pragma once

#include <ix/types.h>

struct pci_bar {
	uint64_t start;	/* the start address, or zero if no resource */
	uint64_t len;	/* the length of the resource */
	uint64_t flags; /* Linux resource flags */
};

/* NOTE: these are the same as the Linux PCI sysfs resource flags */
#define PCI_BAR_IO		0x00000100
#define PCI_BAR_MEM		0x00000200
#define PCI_BAR_PREFETCH	0x00002000 /* typically WC memory */
#define PCI_BAR_READONLY	0x00004000 /* typically option ROMs */

#define PCI_MAX_BARS 7

struct pci_addr {
	uint16_t domain;
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
};

struct pci_dev {
	struct pci_addr addr;

	uint16_t vendor_id;
	uint16_t device_id;
	uint16_t subsystem_vendor_id;
	uint16_t subsystem_device_id;

	struct pci_bar bars[PCI_MAX_BARS];
	int numa_node;
	int max_vfs;
};

extern struct pci_dev *pci_alloc_dev(const struct pci_addr *addr);
extern struct pci_bar *pci_find_mem_bar(struct pci_dev *dev, int count);
extern void *pci_map_mem_bar(struct pci_dev *dev, struct pci_bar *bar, bool wc);
extern void pci_unmap_mem_bar(struct pci_bar *bar, void *vaddr);

