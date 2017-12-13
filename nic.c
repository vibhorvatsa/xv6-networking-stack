#include "nic.h"
#include "defs.h"

int get_device(char* interface, struct nic_device** nd) {
  cprintf("get device for interface=%s\n", interface);
  /**
   *TODO: Use interface name to fetch device details
   *from a table of loaded devices.
   *
   * For now, since we have only one device loaded at a time,
   * this will suffice
   */
   if(nic_devices[0].send_packet == 0 || nic_devices[0].recv_packet == 0) {
     return -1;
   }
   *nd = &nic_devices[0];

   return 0;
}

void register_device(struct nic_device nd) {
  nic_devices[0] = nd;
}
