#include "types.h"
#include "user.h"

int main(void) {
  int MAC_SIZE = 18;
  char* ip = "192.168.2.1";
  char* mac = malloc(MAC_SIZE);
  if(arp(ip, mac, MAC_SIZE) < 0) {
    printf(1, "ARP for IP:%s Failed.\n", ip);
  }
  exit();
}
