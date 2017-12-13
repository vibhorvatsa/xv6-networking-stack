/**
 * author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 * Patterned after https://github.com/wh5a/jos/blob/master/kern/e100.c
 * The network device is different from what is used here. device e1000 from
 * qemu(what we use) gives product id =0x100e = 82540EM Gigabit Ethernet
 * Controller but the referenced code uses 0x1209 = 8255xER/82551IT Fast
 * Ethernet Controller which is device i82550 in qemu
 */

#include "e1000.h"
#include "defs.h"
#include "x86.h"
#include "arp_frame.h"
#include "nic.h"
#include "memlayout.h"

#define E1000_RBD_SLOTS			32
#define E1000_TBD_SLOTS			32

//Bit 31:20 are not writable. Always read 0b.
#define E1000_IOADDR_OFFSET 0x00000000

#define E1000_IODATA_OFFSET 0x00000004

/**
 * Ethernet Device Control Register values
 */
#define E1000_CNTRL_REG           0x00000

//Global device reset. does not clear PCI config
#define E1000_CNTRL_RST_MASK      0x04000000
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
#define E1000_EERD_DATA_BIT_SHIFT   16
#define E1000_EERD_DONE_BIT_MASK    0x00000010

#define E1000_EERD_ADDR(addr) \
        ((addr << E1000_EERD_ADDR_BIT_SHIFT) & E1000_EERD_ADDR_BIT_MASK)

#define E1000_EERD_DATA(eerd) \
        (eerd >> E1000_EERD_DATA_BIT_SHIFT)

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
//__attribute__ ((aligned (16)))
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
//__attribute__ ((aligned (16)))
struct e1000_rbd {
  uint32_t addr_l;
  uint32_t addr_h;
	uint16_t	length;
	uint16_t	checksum;
	uint8_t	status;
	uint8_t	errors;
	uint16_t	special;
};

struct packet_buf {
    uint8_t buf[2046];
};

struct e1000 {
	struct e1000_tbd *tbd[E1000_TBD_SLOTS];
	struct e1000_rbd *rbd[E1000_RBD_SLOTS];

  struct packet_buf *tx_buf[E1000_TBD_SLOTS];  //packet buffer space for tbd
  struct packet_buf *rx_buf[E1000_RBD_SLOTS];  //packet buffer space for rbd

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
};

static void e1000_reg_write(uint32_t reg_addr, uint32_t value, struct e1000 *the_e1000) {
  *(uint32_t*)(the_e1000->membase + reg_addr) = value;
}

static uint32_t e1000_reg_read(uint32_t reg_addr, struct e1000 *the_e1000) {
  uint32_t value = *(uint32_t*)(the_e1000->membase + reg_addr);
  //cprintf("Read value 0x%x from E1000 I/O port 0x%x\n", value, reg_addr);

  return value;
}

// Each inb of port 0x84 takes about 1.25us
// Super stupid delay logic. Don't even know if this works
// or understand what port 0x84 does.
// Could not find an explanantion.
static void udelay(unsigned int u)
{
	unsigned int i;
	for (i = 0; i < u; i++)
		inb(0x84);
}

int e1000_init(struct pci_func *pcif) {
  struct e1000 *the_e1000 = (struct e1000*)kalloc();

	for (int i = 0; i < 6; i++) {
    // I/O port numbers are 16 bits, so they should be between 0 and 0xffff.
    if (pcif->reg_base[i] <= 0xffff) {
      the_e1000->iobase = pcif->reg_base[i];
      if(pcif->reg_size[i] != 64) {  // CSR is 64-byte
        panic("I/O space BAR size != 64");
      }
      break;
    } else if (pcif->reg_base[i] > 0) {
      the_e1000->membase = pcif->reg_base[i];
      if(pcif->reg_size[i] != (1<<17)) {  // CSR is 64-byte
        panic("Mem space BAR size != 128KB");
      }
    }
  }
  if (!the_e1000->iobase)
    panic("Fail to find a valid I/O port base for E1000.");
    if (!the_e1000->membase)
      panic("Fail to find a valid Mem I/O base for E1000.");

	the_e1000->irq_line = pcif->irq_line;

  // Reset device but keep the PCI config
  e1000_reg_write(E1000_CNTRL_REG,
    e1000_reg_read(E1000_CNTRL_REG, the_e1000) | E1000_CNTRL_RST_MASK,
    the_e1000);
  //read back the value after approx 1us to check RST bit cleared
  do {
    udelay(3);
  }while(E1000_CNTRL_RST_BIT(e1000_reg_read(E1000_CNTRL_REG, the_e1000)));

  //the manual says in Section 14.3 General Config -
  //Must set the ASDE and SLU(bit 5 and 6(0 based index)) in the CNTRL Reg to allow auto speed
  //detection after RESET
  uint32_t cntrl_reg = e1000_reg_read(E1000_CNTRL_REG, the_e1000);
  e1000_reg_write(E1000_CNTRL_REG, cntrl_reg | E1000_CNTRL_ASDE_MASK | E1000_CNTRL_SLU_MASK,
    the_e1000);

  //Read Hardware(MAC) address from the device
  uint32_t macaddr_l = e1000_reg_read(E1000_RCV_RAL0, the_e1000);
  uint32_t macaddr_h = e1000_reg_read(E1000_RCV_RAH0, the_e1000);
  for(int i=0,j=1;i<2;i++,j--)
    *(&the_e1000->mac_addr[i]) = (macaddr_h >> (8*j));
  for(int i=0,j=3;i<4;i++,j--)
    *(&the_e1000->mac_addr[i+2]) = (macaddr_l >> (8*j));

  //Transmit/Receive and DMA config beyond this point...
  //sizeof(tbd)=128 bytes. so 32 of these will fit in a page of size 4KB
  struct e1000_tbd *ttmp = (struct e1000_tbd*)kalloc();
  for(int i=0;i<E1000_TBD_SLOTS;i++, ttmp++) {
    the_e1000->tbd[i] = (struct e1000_tbd*)ttmp;
  }
  //check the last nibble of the transmit/receive rings to make sure they
  //are on paragraph boundary
  if( (V2P(the_e1000->tbd[0]) & 0x0000000f) != 0){
    cprintf("ERROR:e1000:Transmit Descriptor Ring not on paragraph boundary\n");
    kfree((char*)ttmp);
    return -1;
  }
  //same for rbd
  struct e1000_rbd *rtmp = (struct e1000_rbd*)kalloc();
  for(int i=0;i<E1000_RBD_SLOTS;i++, rtmp++) {
    the_e1000->rbd[i] = (struct e1000_rbd*)rtmp;
  }
  if( (V2P(the_e1000->rbd[0]) & 0x0000000f) != 0){
    cprintf("ERROR:e1000:Receive Descriptor Ring not on paragraph boundary\n");
    kfree((char*)rtmp);
    return -1;
  }

  //Now for the packet buffers in Receive Ring. Can fit 2 packet buf in 1 page
  struct packet_buf *tmp;
  for(int i=0; i<E1000_RBD_SLOTS; i+=2) {
    tmp = (struct packet_buf*)kalloc();
    the_e1000->rx_buf[i] = tmp++;
    the_e1000->rbd[i]->addr_l = (uint32_t)the_e1000->rx_buf[i];
    the_e1000->rx_buf[i+1] = tmp;
    the_e1000->rbd[i+1]->addr_l = (uint32_t)the_e1000->rx_buf[i+1];
  }
  for(int i=0; i<E1000_TBD_SLOTS; i+=2) {
    tmp = (struct packet_buf*)kalloc();
    the_e1000->tx_buf[i] = tmp++;
    the_e1000->tbd[i]->addr_l = (uint32_t)the_e1000->tx_buf[i];
    the_e1000->tx_buf[i+1] = tmp;
    the_e1000->tbd[i+1]->addr_l = (uint32_t)the_e1000->tx_buf[i+1];
  }

  //Register interrupt handler here...

  return 0;
}

int e1000_send(struct ethr_hdr eth) {
  return 0;
}

int e1000_recv(struct ethr_hdr eth) {
  return 0;
}
