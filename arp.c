/**
 *author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 *kernel code to send recv arp request responses
 */

#include "types.h"
#include "defs.h"
#include "arp_frame.h"
#include "nic.h"

static int block_until_arp_reply(struct ethr_hdr *arpReply) {
  /**
   *TODO: repeated sleep. wake up on each network interrupt.
   *      check for ARP reply for this request.
   *      If received, unblock. else, sleep again.
   */
  return 0;
}

int send_arpRequest(char* interface, char* ipAddr, char* arpResp) {
  cprintf("Create arp request for ip:%s over Interface:%s\n", ipAddr, interface);

  struct nic_device *nd;
  if(get_device(interface, &nd) < 0) {
    cprintf("ERROR:send_arpRequest:Device not loaded\n");
    return -1;
  }

  struct ethr_hdr eth;
  create_eth_arp_frame(nd->mac_addr, ipAddr, &eth);
  nd->send_packet(nd->driver, (uint8_t*)&eth, sizeof(eth)-2); //sizeof(eth)-2 to remove padding. padding was necessary for alignment.

  struct ethr_hdr arpResponse;
  if(block_until_arp_reply(&arpResponse) < 0) {
    cprintf("ERROR:send_arpRequest:Failed to recv ARP response over the NIC\n");
    return -3;
  }

  unpack_mac(arpResponse.arp_dmac, arpResp);
  arpResp[17] = '\0';

  return 0;
}
