#ifndef __XV6_NETSTACK_e1000_H__
#define __XV6_NETSTACK_e1000_H__
/**
 *author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 *device driver for the E1000 emulated NIC
 */
#include "types.h"
#include "nic.h"
#include "pci.h"

struct send_descriptor {

};

struct recv_descriptor {

};

int e1000_init(struct pci_func *pcif);

int e1000_send(struct ethr_hdr);
int e1000_recv(struct ethr_hdr);

#endif
