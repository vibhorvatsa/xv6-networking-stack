#include "pci.h"
#include "x86.h"
#include "defs.h"
#include "pciregisters.h"
#include "e1000.h"
#include "nic.h"

static const char *pci_class[] = {"Unclassified device", "Mass storage controller", "Network controller", "Display controller", "Multimedia device", "Memory controller", "Bridge device"};

static void pci_print_func(struct pci_func *f) {
	const char *class = pci_class[0];
  //Ensure the dev_class code is within the index range of pci_class
	if (PCI_CLASS(f->dev_class) < sizeof(pci_class) / sizeof(pci_class[0]))
		class = pci_class[PCI_CLASS(f->dev_class)];

	cprintf("PCI: %x:%x.%d: %x:%x: class: %x.%x (%s) irq: %d\n",
		f->bus->busno, f->dev, f->func,
		PCI_VENDOR(f->dev_id), PCI_PRODUCT(f->dev_id),
		PCI_CLASS(f->dev_class), PCI_SUBCLASS(f->dev_class), class,
		f->irq_line);
}

static uint32_t pci_config_format_addr(uint32_t bus_addr,
		   uint32_t dev_addr,
		   uint32_t func_addr,
		   uint32_t offset) {

	uint32_t value = 0x80000000 | bus_addr << 16 | dev_addr << 11 | func_addr <<  8 | offset;
	return value;
}

/**
 * write the address of the device + offset(=Config Header register address)you want to read
 * then read the data from the data port
 */
static uint32_t pci_conf_read(struct pci_func *f, uint32_t off) {
	uint32_t value = pci_config_format_addr(f->bus->busno, f->dev, f->func, off);
  outl(PCI_CONFIG_ADDRESS_IOPORT, value);

	return inl(PCI_CONFIG_DATA_IOPORT);
}

static void pci_conf_write(struct pci_func *f, uint32_t off, uint32_t v) {
	uint32_t address = pci_config_format_addr(f->bus->busno, f->dev, f->func, off);
	outl(PCI_CONFIG_ADDRESS_IOPORT, address);

	outl(PCI_CONFIG_DATA_IOPORT, v);
}

void pci_enable_device(struct pci_func *f) {

	pci_conf_write(f, PCI_COMMAND_STATUS_REG,
		       PCI_COMMAND_IO_ENABLE |
		       PCI_COMMAND_MEM_ENABLE |
		       PCI_COMMAND_MASTER_ENABLE);
					 cprintf("pcicmd reg:0x%x\n", pci_conf_read(f, PCI_COMMAND_STATUS_REG));

	uint32_t bar_width;
	uint32_t bar;
	for (bar = PCI_MAPREG_START; bar < PCI_MAPREG_END; bar += bar_width) {
		uint32_t oldv = pci_conf_read(f, bar);

		bar_width = 4;

		/**
		 * To determine the amount of address space needed by a PCI device,
		 * you must save the original value of the BAR, write a value of all 1's
		 * to the register, then read it back. The amount of memory can then be
		 * determined by masking the information bits, performing a
		 * bitwise NOT ('~' in C), and incrementing the value by 1.
		 *
		 * http://wiki.osdev.org/PCI
		 */
		pci_conf_write(f, bar, 0xffffffff);
		uint32_t rv = pci_conf_read(f, bar);

		if (rv == 0)
			continue;

		int regnum = PCI_MAPREG_NUM(bar);
		uint32_t base, size;
		if (PCI_MAPREG_TYPE(rv) == PCI_MAPREG_TYPE_MEM) {
			if (PCI_MAPREG_MEM_TYPE(rv) == PCI_MAPREG_MEM_TYPE_64BIT)
				bar_width = 8;

			size = PCI_MAPREG_MEM_SIZE(rv);
			base = PCI_MAPREG_MEM_ADDR(oldv);
			cprintf("mem region %d: %d bytes at 0x%x\n",
					regnum, size, base);
		} else {
			size = PCI_MAPREG_IO_SIZE(rv);
			base = PCI_MAPREG_IO_ADDR(oldv);
			cprintf("io region %d: %d bytes at 0x%x\n",
					regnum, size, base);
		}

		pci_conf_write(f, bar, oldv);
		f->reg_base[regnum] = base;
		f->reg_size[regnum] = size;

		if (size && !base)
			cprintf("PCI device %x:%x.%d (%x:%x) "
				"may be misconfigured: "
				"region %d: base 0x%x, size %d\n",
				f->bus->busno, f->dev, f->func,
				PCI_VENDOR(f->dev_id), PCI_PRODUCT(f->dev_id),
				regnum, base, size);
	}
}

static int e1000_attach(struct pci_func *pcif) {
	pci_enable_device(pcif);
	struct nic_device nd;
	e1000_init(pcif, &nd.driver, nd.mac_addr);
	nd.send_packet = e1000_send;
	nd.recv_packet = e1000_recv;
	register_device(nd);
  return 0;
}

struct pci_driver pci_attach_vendor_based[] = {
  { 0x8086, 0x100e, e1000_attach},
	{ 0, 0, 0 },
};

/**
 * Attach the given PCI device with its driver based on vendor
 */
static void pci_attach_nic(struct pci_func *pcif) {
  uint32_t vendor_id = PCI_VENDOR(pcif->dev_id);
  uint32_t product_id = PCI_PRODUCT(pcif->dev_id);
  struct pci_driver *list = &pci_attach_vendor_based[0];

	for (uint i = 0; list[i].attachfn; i++) {
		if (list[i].key1 == vendor_id && list[i].key2 == product_id) {
			int r = list[i].attachfn(pcif);

			if (r < 0)
				cprintf("pci_attach_match: attaching "
					"%x.%x (%p): %e\n",
					vendor_id, product_id, list[i].attachfn, r);
		}
	}
}

static int pci_enumerate_bus(struct pci_bus *bus) {
	int totaldev = 0;
	struct pci_func df;
	memset(&df, 0, sizeof(df));
	df.bus = bus;

  //Enumerate over the root PCI bus, and for each device_number
  //check if we have a supported device connected.
  // If yes, configure the device.
	for (df.dev = 0; df.dev < MAX_DEVICE_PER_PCI_BUS; df.dev++) {
		uint32_t bhlc = pci_conf_read(&df, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlc) > 1)	// only supporting PCI-2-PCI bus which is HDRTYPE=1. Unsupported or no device
			continue;

		totaldev++;

		struct pci_func f = df;
    //if the PCI device is Multi-function device(indicated by setting HDR Register Most Sig bit 1)
    //Then there can be atmost 2^3=8 functions(because PCI bus address has 3 bits for func addr)
		for (f.func = 0; f.func < (PCI_HDRTYPE_MULTIFN(bhlc) ? 8 : 1); f.func++) {
			struct pci_func af = f;

      // read the device id
			af.dev_id = pci_conf_read(&f, PCI_ID_REG);
			if (PCI_VENDOR(af.dev_id) == 0xffff) // all bits set is Invalid device id. so assume no device
				continue;

      // read the interrupt line... assuming only one interrupt pin???
			uint32_t intr = pci_conf_read(&af, PCI_INTERRUPT_REG);
			af.irq_line = PCI_INTERRUPT_LINE(intr);
			af.irq_pin = PCI_INTERRUPT_PIN(intr);

      //read the full device class_code + subclass + progIF + Revision_id
			af.dev_class = pci_conf_read(&af, PCI_CLASS_REG);

      pci_print_func(&af);  //print it for debugging

      if(PCI_CLASS(af.dev_class) == PCI_DEVICE_CLASS_NETWORK_CONTROLLER)
			   pci_attach_nic(&af);
		}
	}

	return totaldev;
}

int pci_init(void) {
	static struct pci_bus root_bus;
	memset(&root_bus, 0, sizeof(root_bus));

	return pci_enumerate_bus(&root_bus);
}
