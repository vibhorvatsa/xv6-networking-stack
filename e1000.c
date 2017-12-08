#include "e1000.h"
#include "defs.h"
#include "x86.h"

#define E1000_RBD_SLOTS			64
#define E1000_TBD_SLOTS			64

//Bit 31:20 are not writable. Always read 0b.
#define E1000_IOADDR_OFFSET 0x00000000

#define E1000_IODATA_OFFSET 0x00000004

/**
 * Ethernet Device Control Register values
 */
#define E1000_CNTRL_REG     0x00000

#define E1000_CNTRL_LRST_MASK     0x00000004
#define E1000_CNTRL_RST_MASK      0x02000000
#define E1000_CNTRL_PHY_RST_MASK  0x80000000

#define E1000_CNTRL_ASDE_MASK     0x00000010
#define E1000_CNTRL_SLU_MASK      0x00000020

#define E1000_CNTRL_RST_BIT(cntrl) \
        (cntrl & E1000_CNTRL_RST_MASK)


//Trasmit Buffer Descriptor
struct e1000_tbd {

};

//Receive Buffer Descriptor
struct e1000_rbd {

};

static struct {
  // The Transmit Descriptor Queue must be aligned on 16-byte boundary
	struct e1000_tbd tbd[E1000_TBD_SLOTS];
	int tbd_head;
	int tbd_tail;
	char tbd_idle;

	struct e1000_rbd rbd[E1000_RBD_SLOTS];
	int rbd_head;
	int rbd_tail;
	char rbd_idle;

  uint32_t iobase;
  uint8_t irq_line;
} the_e1000;

static void e1000_reg_write(uint32_t reg_addr, uint32_t value) {
  //First write the reg_addr you want to write the data to, into IOADDR
  outl(the_e1000.iobase + E1000_IOADDR_OFFSET, reg_addr);

  //Then write the value in to IODATA
  outl(the_e1000.iobase + E1000_IODATA_OFFSET, value);
}

static uint32_t e1000_reg_read(uint32_t reg_addr) {
  //First write the reg_addr you want to read the data from, into IOADDR
  outl(the_e1000.iobase + E1000_IOADDR_OFFSET, reg_addr);

  //Then read the value in from IODATA
  return inl(the_e1000.iobase + E1000_IODATA_OFFSET);
}

// Each inb of port 0x84 takes about 1.25us
//Super stupid delay logic. Don't even know if this works
// or understand what port 0x84 does.
// Could not find an explanantion.
static void udelay(unsigned int u)
{
	unsigned int i;
	for (i = 0; i < u; i++)
		inb(0x84);
}

int e1000_init(struct pci_func *pcif) {
	for (int i = 0; i < 6; i++) {
    // I/O port numbers are 16 bits, so they should be between 0 and 0xffff.
    if (pcif->reg_base[i] <= 0xffff) {
      the_e1000.iobase = pcif->reg_base[i];
      if(pcif->reg_size[i] != 64) {  // CSR is 64-byte
        panic("I/O space BAR size != 64");
      }
      break;
    }
  }
  if (!the_e1000.iobase)
    panic("Fail to find a valid I/O port base for E1000.");

	the_e1000.irq_line = pcif->irq_line;

  // Reset device but keep the PCI config
  e1000_reg_write(E1000_CNTRL_REG, E1000_CNTRL_RST_MASK);
  //read back the value after approx 1us to check RST bit cleared
  do {
    udelay(1);
  }while(E1000_CNTRL_RST_BIT(e1000_reg_read(E1000_CNTRL_REG)));

  //Must set the ASDE and SLU(bit 5 and 6) in the CNRTL Reg to allow auto speed detection
  //Or so the document says in Section 14.3 General Config. JOS code did not do it, but I think the network device is
  // different from what we use. device e1000 from qemu(what we use) gives product id =0x100e
  // but JOS uses 0x1209 which is 8255xER/82551IT Fast Ethernet Controller
  e1000_reg_write(E1000_CNTRL_REG, E1000_CNTRL_ASDE_MASK);
  e1000_reg_write(E1000_CNTRL_REG, E1000_CNTRL_SLU_MASK);

  //Transmit/Receive and DMA config beyond this point...

  return 0;
}

int e1000_send(struct ethr_hdr eth) {
  return 0;
}

int e1000_recv(struct ethr_hdr eth) {
  return 0;
}
