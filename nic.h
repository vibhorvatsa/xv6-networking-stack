#ifndef __XV6_NETSTACK_NIC_H__
#define __XV6_NETSTACK_NIC_H__
/**
 *author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 *load device drivers for different NICs
 */

#include "types.h"
#include "arp_frame.h"

//Generic NIC device driver container
struct nic_driver {
  int (*send_packet) (struct ethr_hdr packet);
  int (*recv_packet) (struct ethr_hdr packet);
};

//Holds the instances of nic_driver for loaded drivers
//Lets say for now there can't be more than 1 loaded NIC driver
struct nic_driver nic_drivers[1];

void register_driver(struct nic_driver nd);
int get_device_driver(char* interface, struct nic_driver** nd);

#endif
