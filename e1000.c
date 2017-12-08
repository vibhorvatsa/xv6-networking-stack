#include "e1000.h"
#include "defs.h"

#define E1000_RBD_SLOTS			64
#define E1000_TBD_SLOTS			64

//Trasmit Buffer Descriptor
struct e1000_tbd {

};

//Receive Buffer Descriptor
struct e1000_rbd {

};

static struct {
	uint32_t iobase;
  uint8_t irq_line;

	struct e1000_tbd tbd[E1000_TBD_SLOTS];
	int tbd_head;
	int tbd_tail;
	char tbd_idle;

	struct e1000_rbd rbd[E1000_RBD_SLOTS];
	int rbd_head;
	int rbd_tail;
	char rbd_idle;
} the_e1000;

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

  return 0;
}

int e1000_send(struct ethr_hdr eth) {
  return 0;
}

int e1000_recv(struct ethr_hdr eth) {
  return 0;
}
