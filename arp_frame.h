#ifndef __XV6_NETSTACK_ARP_FRAME_H__
#define __XV6_NETSTACK_ARP_FRAME_H__
/**
 *edit1: Anmol Vatsa<anvatsa@cs.utah.edu>
 *take stuff from the c file and put it here for includes
 */

struct arp_ipv4 {
	uchar smac[6];
	uint sip;
	uchar dmac[6];
	uint dip;
};

struct arp_hdr {
	uint hwtype;
	uint protype;
	char hwsize;
	char prosize;
	uint opcode;
	struct arp_ipv4 arp_data;
};


struct ethr_hdr {
	uchar dmac[6];
	uchar smac[6];
	char ethr_type[2];
	struct arp_hdr arp;
};

int create_eth_arp_frame(char* ipAddr, struct ethr_hdr *eth);
void unpack_mac(uchar* mac, char* mac_str);
char int_to_hex (uint n);

#endif
