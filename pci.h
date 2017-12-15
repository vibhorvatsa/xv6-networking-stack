#ifndef __XV6_NETSTACK_PCI_H__
#define __XV6_NETSTACK_PCI_H__
/**
 * author: Anmol Vatsa <anvatsa@cs.utah.edu>
 *
 * Header file for walking the PCI bus and finding devices
 * PCI device classes taken from https://pci-ids.ucw.cz/read/PD/
 *
 */

#include "types.h"

// Since PCI bus addresses have 8-bit for PCI bus,
// 5-bit for device , and 3-bit for function numbers for the device
// So a total of 2^5 devices per bus
#define MAX_DEVICE_PER_PCI_BUS 32


// http://en.wikipedia.org/wiki/PCI_Configuration_Space#Software_implementation
// configure a PCI device by writing the address of the device's register into CONFIG_ADDRESS
#define PCI_CONFIG_ADDRESS_IOPORT 0xCF8
// put the data that is supposed to be written to the device into CONFIG_DATA
#define PCI_CONFIG_DATA_IOPORT 0xCFC

struct pci_func;
/**
 * We consider only one PCI bus
 */
struct pci_bus {
    struct pci_func *parent_bridge;
    uint32_t busno;
};

struct pci_func {
    struct pci_bus *bus;	// Primary bus for bridges

    uint32_t dev; //device number, sequentially allocated to devices on the bus
    uint32_t func; //function number, sequentially allocated for multi-func devices

    uint32_t dev_id;
    uint32_t dev_class;

    uint32_t reg_base[6];
    uint32_t reg_size[6];
    uint8_t irq_line;
    uint8_t irq_pin;
};

// PCI driver container
struct pci_driver {
	uint32_t key1, key2;

	int (*attachfn) (struct pci_func *pcif);
};

int pci_init(void);

#endif
