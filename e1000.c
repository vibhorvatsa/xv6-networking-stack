/**
 * author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 * Patterned after https://github.com/wh5a/jos/blob/master/kern/e100.c
 * The network device is different from what we use. device e1000 from
 * qemu(what we use) gives product id =0x100e = 82540EM Gigabit Ethernet
 * Controller but the referenced code uses 0x1209 = 8255xER/82551IT Fast
 * Ethernet Controller which is device i82550 in qemu
 */

#include "e1000.h"
#include "defs.h"
#include "x86.h"
#include "arp_frame.h"

#define E1000_RBD_SLOTS			64
#define E1000_TBD_SLOTS			64

//Bit 31:20 are not writable. Always read 0b.
#define E1000_IOADDR_OFFSET 0x00000000

#define E1000_IODATA_OFFSET 0x00000004

/**
 * Ethernet Device Control Register values
 */
#define E1000_CNTRL_REG           0x00000

//Global device reset. does not clear PCI config
#define E1000_CNTRL_RST_MASK      0x02000000
#define E1000_CNTRL_ASDE_MASK     0x00000020
#define E1000_CNTRL_SLU_MASK      0x00000040


#define E1000_CNTRL_RST_BIT(cntrl) \
        (cntrl & E1000_CNTRL_RST_MASK)

/**
 * Ethernet Device Receive registers
 */
 #define E1000_RCV_RAL0     0x05400
 #define E1000_RCV_RAH0     0x05404

 /**
  * Ethernet Device EEPROM registers
  */
#define E1000_EERD_REG_ADDR         0x00014

#define E1000_EERD_START_BIT_MASK   0x00000001
#define E1000_EERD_ADDR_BIT_MASK    0x0000ff00
#define E1000_EERD_ADDR_BIT_SHIFT   8
#define E1000_EERD_DATA_BIT_MASK    0xffff0000
#define E1000_EERD_DONE_BIT_MASK    0x00000010

#define E1000_EERD_ADDR(addr) \
        ((addr << E1000_EERD_ADDR_BIT_SHIFT) & E1000_EERD_ADDR_BIT_MASK)

#define E1000_EERD_DATA(eerd) \
        (eerd & E1000_EERD_DATA_BIT_MASK)

#define E1000_EERD_DONE(eerd) \
        (eerd & E1000_EERD_DONE_BIT_MASK)

/**
 * EEPROM Address Map
 */
#define EEPROM_ADDR_MAP_ETHER_ADDR_1    0x00
#define EEPROM_ADDR_MAP_ETHER_ADDR_2    0x01
#define EEPROM_ADDR_MAP_ETHER_ADDR_3    0x02

//Trasmit Buffer Descriptor
// The Transmit Descriptor Queue must be aligned on 16-byte boundary
__attribute__ ((aligned (16)))
struct e1000_tbd {
  uint32_t addr_l;
  uint32_t addr_h;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

//Receive Buffer Descriptor
// The Receive Descriptor Queue must be aligned on 16-byte boundary
__attribute__ ((aligned (16)))
struct e1000_rbd {

};

static struct {
	struct e1000_tbd tbd[E1000_TBD_SLOTS];
	struct e1000_rbd rbd[E1000_RBD_SLOTS];

  int tbd_head;
	int tbd_tail;
	char tbd_idle;

	int rbd_head;
	int rbd_tail;
	char rbd_idle;

  uint32_t iobase;
  uint32_t membase;
  uint8_t irq_line;
  uint8_t mac_addr[6];
} the_e1000;

static void e1000_reg_write(uint32_t reg_addr, uint32_t value) {
  cprintf("Writing value 0x%x to E1000 I/O port 0x%x\n", value, reg_addr);
  //First write the reg_addr you want to write the data to, into IOADDR
  outl(the_e1000.iobase + E1000_IOADDR_OFFSET, reg_addr);
cprintf("Written address 0x%x to E1000_IOADDR=0x%x\n", inl(the_e1000.iobase + E1000_IOADDR_OFFSET), the_e1000.iobase + E1000_IOADDR_OFFSET);
  //Then write the value in to IODATA
  outl(the_e1000.iobase + E1000_IODATA_OFFSET, value);
  cprintf("Written value 0x%x to reg=0x%x\n", inl(the_e1000.iobase + E1000_IODATA_OFFSET), reg_addr);
}

static uint32_t e1000_reg_read(uint32_t reg_addr) {
  //First write the reg_addr you want to read the data from, into IOADDR
  outl(the_e1000.iobase + E1000_IOADDR_OFFSET, reg_addr);

  //Then read the value in from IODATA
  uint32_t value = inl(the_e1000.iobase + E1000_IODATA_OFFSET);
  cprintf("Read value 0x%x from E1000 I/O port 0x%x\n", value, reg_addr);

  return value;
}
/*
// Each inb of port 0x84 takes about 1.25us
// Super stupid delay logic. Don't even know if this works
// or understand what port 0x84 does.
// Could not find an explanantion.
static void udelay(unsigned int u)
{
	unsigned int i;
	for (i = 0; i < u; i++)
		inb(0x84);
}*/

int e1000_init(struct pci_func *pcif) {
  //check the last nibble of the transmit/receive rings to make sure they
  //are on paragraph boundary
  // TODO: Shouldn't this check be on Physical addresses???
  if((((uint32_t)the_e1000.tbd) & 0x0000000f) != 0){
    cprintf("ERROR:e1000:Transmit Descriptor Ring not on paragraph boundary\n");
    return -1;
  }
  if((((uint32_t)the_e1000.rbd) & 0x0000000f) != 0){
    cprintf("ERROR:e1000:Receive Descriptor Ring not on paragraph boundary\n");
    return -1;
  }

	for (int i = 0; i < 6; i++) {
    // I/O port numbers are 16 bits, so they should be between 0 and 0xffff.
    if (pcif->reg_base[i] <= 0xffff) {
      the_e1000.iobase = pcif->reg_base[i];
      if(pcif->reg_size[i] != 64) {  // CSR is 64-byte
        panic("I/O space BAR size != 64");
      }
      break;
    } else if (pcif->reg_base[i] > 0) {
      the_e1000.membase = pcif->reg_base[i];
      if(pcif->reg_size[i] != (1<<17)) {  // CSR is 64-byte
        panic("Mem space BAR size != 128KB");
      }
    }
  }
  if (!the_e1000.iobase)
    panic("Fail to find a valid I/O port base for E1000.");

	the_e1000.irq_line = pcif->irq_line;
/*
  // Reset device but keep the PCI config
  e1000_reg_write(E1000_CNTRL_REG, E1000_CNTRL_RST_MASK);
  //read back the value after approx 1us to check RST bit cleared
  do {
    udelay(1);
  }while(E1000_CNTRL_RST_BIT(e1000_reg_read(E1000_CNTRL_REG)));
*/
  //the manual says in Section 14.3 General Config -
  //Must set the ASDE and SLU(bit 5 and 6) in the CNTRL Reg to allow auto speed
  //detection after RESET
  uint32_t cntrl_reg = e1000_reg_read(E1000_CNTRL_REG);
  e1000_reg_write(E1000_CNTRL_REG, cntrl_reg | E1000_CNTRL_ASDE_MASK | E1000_CNTRL_SLU_MASK);

  //Read Hardware(MAC) address from the device EEPROM's first 3 words
//  uint16_t macddr_1, macddr_2, macddr_3;
  e1000_reg_write(E1000_EERD_REG_ADDR,
    E1000_EERD_ADDR(EEPROM_ADDR_MAP_ETHER_ADDR_1) | E1000_EERD_START_BIT_MASK);

    
/*  //Read the DONE Bit until it reads 1b to ensure DATA available
  do {
    macddr_1 = e1000_reg_read(E1000_EERD_REG_ADDR);
  }while(!E1000_EERD_DONE(macddr_1));
  macddr_1 = E1000_EERD_DATA(macddr_1);
  cprintf("MAC Address of e1000 device from EEPROM word 3:%x\n", macddr_1);
  e1000_reg_write(E1000_EERD_REG_ADDR,
    E1000_EERD_ADDR(EEPROM_ADDR_MAP_ETHER_ADDR_2) | E1000_EERD_START_BIT_MASK);
  do {
    macddr_2 = e1000_reg_read(E1000_EERD_REG_ADDR);
  }while(!E1000_EERD_DONE(macddr_2));
  macddr_2 = E1000_EERD_DATA(macddr_2);

  e1000_reg_write(E1000_EERD_REG_ADDR,
    E1000_EERD_ADDR(EEPROM_ADDR_MAP_ETHER_ADDR_3) | E1000_EERD_START_BIT_MASK);
  do {
    macddr_3 = e1000_reg_read(E1000_EERD_REG_ADDR);
  }while(!E1000_EERD_DONE(macddr_3));
  macddr_3 = E1000_EERD_DATA(macddr_3);
  cprintf("MAC Address of e1000 device from EEPROM word 3:%x\n", macddr_3);
  uint32_t macaddr_l = e1000_reg_read(E1000_RCV_RAL0);
  uint32_t macaddr_h = e1000_reg_read(E1000_RCV_RAH0);
  cprintf("MAC Address of e1000 device:%x\n", (macaddr_l | (macaddr_h & 0xff)));
  *the_e1000.mac_addr = macaddr_l | (macaddr_h & 0xff);
  char mac_str[18];
  unpack_mac(the_e1000.mac_addr, mac_str);
  mac_str[17] = 0;
  cprintf("MAC string of e1000 device:%s\n", mac_str);
  //Transmit/Receive and DMA config beyond this point...
*/
  //Register interrupt handler here...

  return 0;
}

int e1000_send(struct ethr_hdr eth) {
  return 0;
}

int e1000_recv(struct ethr_hdr eth) {
  return 0;
}
