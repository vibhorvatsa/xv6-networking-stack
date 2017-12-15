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

#define E1000_RBD_SLOTS			128
#define E1000_TBD_SLOTS			128

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
 * Ethernet Device registers
 */
#define E1000_RCV_RAL0      0x05400
#define E1000_RCV_RAH0      0x05404
#define E1000_TDBAL         0x03800
#define E1000_TDBAH         0x03804
#define E1000_TDLEN         0x03808
#define E1000_TDH           0x03810
#define E1000_TDT           0x03818
#define E1000_RDBAL         0x02800
#define E1000_RDBAH         0x02804
#define E1000_RDLEN         0x02808
#define E1000_RDH           0x02810
#define E1000_RDT           0x02818

/**
 * Ethernet Device Transmission Control register
 */
#define E1000_TCTL                0x00400

#define E1000_TCTL_EN             0x00000002
#define E1000_TCTL_PSP            0x00000008
#define E1000_TCTL_CT_BIT_MASK    0x00000ff0
#define E1000_TCTL_CT_BIT_SHIFT   4
#define E1000_TCTL_CT_SET(value) \
        ((value << E1000_TCTL_CT_BIT_SHIFT) & E1000_TCTL_CT_BIT_MASK)
#define E1000_TCTL_COLD_BIT_MASK  0x003ff000
#define E1000_TCTL_COLD_BIT_SHIFT 12
#define E1000_TCTL_COLD_SET(value) \
        ((value << E1000_TCTL_COLD_BIT_SHIFT) & E1000_TCTL_COLD_BIT_MASK)

/**
 * Ethernet Device Transmission Inter-Packet Gap register
 */
#define E1000_TIPG                0x00410

#define E1000_TIPG_IPGT_BIT_MASK    0x000003ff
#define E1000_TIPG_IPGT_BIT_SHIFT   0
#define E1000_TIPG_IPGT_SET(value) \
        ((value << E1000_TIPG_IPGT_BIT_SHIFT) & E1000_TIPG_IPGT_BIT_MASK)
#define E1000_TIPG_IPGR1_BIT_MASK   0x000ffc00
#define E1000_TIPG_IPGR1_BIT_SHIFT   10
#define E1000_TIPG_IPGR1_SET(value) \
        ((value << E1000_TIPG_IPGR1_BIT_SHIFT) & E1000_TIPG_IPGR1_BIT_MASK)
#define E1000_TIPG_IPGR2_BIT_MASK   0x3ff00000
#define E1000_TIPG_IPGR2_BIT_SHIFT   20
#define E1000_TIPG_IPGR2_SET(value) \
        ((value << E1000_TIPG_IPGR2_BIT_SHIFT) & E1000_TIPG_IPGR2_BIT_MASK)

/**
* Ethernet Device Interrupt Mast Set registers
*/
#define E1000_IMS                 0x000d0
#define E1000_IMS_TXQE            0x00000002
#define E1000_IMS_RXSEQ           0x00000008
#define E1000_IMS_RXO             0x00000040
#define E1000_IMS_RXT0            0x00000080

/**
 * Ethernet Device Receive Control register
 */
#define E1000_RCTL                0x00100

#define E1000_RCTL_EN             0x00000002
#define E1000_RCTL_BAM            0x00008000
#define E1000_RCTL_BSIZE          0x00000000
#define E1000_RCTL_SECRC          0x04000000

/**
 * Ethernet Device Transmit Descriptor Command Field
 */
#define E1000_TDESC_CMD_RS      0x08
#define E1000_TDESC_CMD_EOP     0x01
#define E1000_TDESC_CMD_IFCS    0x02

/**
 * Ethernet Device Transmit Descriptor Status Field
 */
#define E1000_TDESC_STATUS_DONE_MASK   0x01
#define E1000_TDESC_STATUS_DONE(status) \
        (status & E1000_TDESC_STATUS_DONE_MASK)

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
__attribute__ ((packed))
struct e1000_tbd {
  uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

//Receive Buffer Descriptor
// The Receive Descriptor Queue must be aligned on 16-byte boundary
__attribute__ ((packed))
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
  uint8_t irq_pin;
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

void e1000_send(void *driver, uint8_t *pkt, uint16_t length )
{
  struct e1000 *e1000 = (struct e1000*)driver;
  cprintf("e1000 driver: Sending packet of length:0x%x %x starting at physical address:0x%x\n", length, sizeof(struct ethr_hdr), V2P(e1000->tx_buf[e1000->tbd_tail]));
  memset(e1000->tbd[e1000->tbd_tail], 0, sizeof(struct e1000_tbd));
  memmove((e1000->tx_buf[e1000->tbd_tail]), pkt, length);
  e1000->tbd[e1000->tbd_tail]->addr = (uint64_t)(uint32_t)V2P(e1000->tx_buf[e1000->tbd_tail]);
	e1000->tbd[e1000->tbd_tail]->length = length;
	e1000->tbd[e1000->tbd_tail]->cmd = (E1000_TDESC_CMD_RS | E1000_TDESC_CMD_EOP | E1000_TDESC_CMD_IFCS);
  e1000->tbd[e1000->tbd_tail]->cso = 0;
	// update the tail so the hardware knows it's ready
	int oldtail = e1000->tbd_tail;
	e1000->tbd_tail = (e1000->tbd_tail + 1) % E1000_TBD_SLOTS;
	e1000_reg_write(E1000_TDT, e1000->tbd_tail, e1000);

	while( !E1000_TDESC_STATUS_DONE(e1000->tbd[oldtail]->status) )
	{
		udelay(2);
	}
  cprintf("after while loop\n");
}

int e1000_init(struct pci_func *pcif, void** driver, uint8_t *mac_addr) {
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
  the_e1000->irq_pin = pcif->irq_pin;
  cprintf("e1000 init: interrupt pin=%d and line:%d\n",the_e1000->irq_pin,the_e1000->irq_line);
  the_e1000->tbd_head = the_e1000->tbd_tail = 0;
  the_e1000->rbd_head = the_e1000->rbd_tail = 0;

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
  *(uint32_t*)the_e1000->mac_addr = macaddr_l;
  *(uint16_t*)(&the_e1000->mac_addr[4]) = (uint16_t)macaddr_h;
  *(uint32_t*)mac_addr = macaddr_l;
  *(uint32_t*)(&mac_addr[4]) = (uint16_t)macaddr_h;
  char mac_str[18];
  unpack_mac(the_e1000->mac_addr, mac_str);
  mac_str[17] = 0;

  cprintf("\nMAC address of the e1000 device:%s\n", mac_str);
  //Transmit/Receive and DMA config beyond this point...
  //sizeof(tbd)=128bits/16bytes. so 256 of these will fit in a page of size 4KB.
  //But the struct e1000 has to contain pointer to these many descriptors.
  //Each pointer being 4 bytes, and 4 such array of pointers(inclusing packet buffers)
  //you get N*16+(some more values in the struct e1000) = 4096
  // N=128=E1000_TBD_SLOTS. i.e., the maximum number of descriptors in one ring
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
    the_e1000->rbd[i]->addr_l = V2P((uint32_t)the_e1000->rx_buf[i]);
    the_e1000->rbd[i]->addr_h = 0;
    the_e1000->rx_buf[i+1] = tmp;
    the_e1000->rbd[i+1]->addr_l = V2P((uint32_t)the_e1000->rx_buf[i+1]);
    the_e1000->rbd[i+1]->addr_h = 0;
  }
  for(int i=0; i<E1000_TBD_SLOTS; i+=2) {
    tmp = (struct packet_buf*)kalloc();
    the_e1000->tx_buf[i] = tmp++;
    //the_e1000->tbd[i]->addr = (uint32_t)the_e1000->tx_buf[i];
    //the_e1000->tbd[i]->addr_h = 0;
    the_e1000->tx_buf[i+1] = tmp;
    //the_e1000->tbd[i+1]->addr_l = (uint32_t)the_e1000->tx_buf[i+1];
    //the_e1000->tbd[i+1]->addr_h = 0;
  }

  //Write the Descriptor ring addresses in TDBAL, and RDBAL, plus HEAD and TAIL pointers
  e1000_reg_write(E1000_TDBAL, V2P(the_e1000->tbd[0]), the_e1000);
  e1000_reg_write(E1000_TDBAH, 0x00000000, the_e1000);
  e1000_reg_write(E1000_TDLEN, (E1000_TBD_SLOTS*16) << 7, the_e1000);
  e1000_reg_write(E1000_TDH, 0x00000000, the_e1000);
  e1000_reg_write(E1000_TCTL,
                  E1000_TCTL_EN |
                    E1000_TCTL_PSP |
                    E1000_TCTL_CT_SET(0x0f) |
                    E1000_TCTL_COLD_SET(0x200),
                  the_e1000);
  e1000_reg_write(E1000_TDT, 0, the_e1000);
  e1000_reg_write(E1000_TIPG,
                  E1000_TIPG_IPGT_SET(10) |
                    E1000_TIPG_IPGR1_SET(10) |
                    E1000_TIPG_IPGR2_SET(10),
                  the_e1000);
  e1000_reg_write(E1000_RDBAL, V2P(the_e1000->rbd[0]), the_e1000);
  e1000_reg_write(E1000_RDBAH, 0x00000000, the_e1000);
  e1000_reg_write(E1000_RDLEN, (E1000_RBD_SLOTS*16) << 7, the_e1000);
  e1000_reg_write(E1000_RDH, 0x00000000, the_e1000);
  e1000_reg_write(E1000_RDT, 0x00000000, the_e1000);
  //enable interrupts
  e1000_reg_write(E1000_IMS, E1000_IMS_RXSEQ | E1000_IMS_RXO | E1000_IMS_RXT0|E1000_IMS_TXQE, the_e1000);
  //Receive control Register.
  e1000_reg_write(E1000_RCTL,
                E1000_RCTL_EN |
                  E1000_RCTL_BAM |
                  E1000_RCTL_BSIZE | 0x00000008,//|
                //  E1000_RCTL_SECRC,
                the_e1000);
cprintf("e1000:Interrupt enabled mask:0x%x\n", e1000_reg_read(E1000_IMS, the_e1000));
  //Register interrupt handler here...
  picenable(the_e1000->irq_line);
  ioapicenable(the_e1000->irq_line, 0);
  ioapicenable(the_e1000->irq_line, 1);


  *driver = the_e1000;
  return 0;
}

void e1000_recv(void *driver, uint8_t* pkt, uint16_t length) {
}
