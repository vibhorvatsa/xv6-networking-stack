/**
 *author: Anmol Vatsa<anvatsa@cs.utah.edu>
 *
 *system call to send an ARP request
 *expects a char* IP address as argument
 */

#include "types.h"
#include "defs.h"

int sys_arp(void) {
  char *ipAddr;
  char *arpResp;
  int size;

  if(argstr(0, &ipAddr) < 0 || argint(2, &size) < 0 || argptr(1, &arpResp, size) < 0) {
    cprintf("ERROR:sys_createARP:Failed to fetch arguments");
    return -1;
  }

  if(send_arpRequest(ipAddr, arpResp) < 0) {
    cprintf("ERROR:sys_createARP:Failed to send ARP Request for IP:%s", ipAddr);
    return -1;
  }

  return 0;
}
