#ifndef __XV6_NETSTACK_e1000_H__
#define __XV6_NETSTACK_e1000_H__
/**
 *author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 *device driver for the E1000 emulated NIC on an x86 core
 *https://pdos.csail.mit.edu/6.828/2017/readings/hardware/8254x_GBe_SDM.pdf
 */
#include "types.h"
#include "nic.h"
#include "pci.h"

struct e1000;

int e1000_init(struct pci_func *pcif, void **driver, uint8_t *mac_addr);

void e1000_send(void *e1000, uint8_t* pkt, uint16_t length);
void e1000_recv(void *e1000, uint8_t* pkt, uint16_t length);

#endif
