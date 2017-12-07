#include "nic.h"
#include "defs.h"

int get_device_driver(char* interface, struct nic_driver** nd) {
  cprintf("get device driver for interface=%s\n", interface);
  /**
   *TODO: Use interface name to fetch device details
   *and driver from a table of loaded drivers.
   *
   * For now, since we have only one driver loaded at a time,
   * this will suffice
   */
   if(nic_drivers[0].send_packet == 0 || nic_drivers[0].recv_packet == 0) {
     return -1;
   }
   *nd = &nic_drivers[0];

   return 0;
}

void register_driver(struct nic_driver nd) {
  nic_drivers[0] = nd;
}
