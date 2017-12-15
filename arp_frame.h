#ifndef __XV6_NETSTACK_ARP_FRAME_H__
#define __XV6_NETSTACK_ARP_FRAME_H__
/**
 *edit1: Anmol Vatsa<anvatsa@cs.utah.edu>
 *take stuff from the c file and put it here for includes
 */

struct ethr_hdr {
	uint8_t dmac[6];
	uint8_t smac[6];
	uint16_t ethr_type;
	uint16_t hwtype;
	uint16_t protype;
	uint8_t hwsize;
	uint8_t prosize;
	uint16_t opcode;
	uint8_t arp_smac[6];
	uint32_t sip;
	uint8_t arp_dmac[6];
	uint16_t dip; //This should be 4 bytes. But alignment issues are creating a padding b/w arp_dmac and dip if dip is kept 4 bytes.
	uint16_t dip2;
	uint16_t padd;//This need not be here explicitly. Compiler automatically inserts padding. But since we are removing padding length from struct length while calculating length, lets keep it here explicitly.
};

int create_eth_arp_frame(uint8_t* smac, char* ipAddr, struct ethr_hdr *eth);
void unpack_mac(uchar* mac, char* mac_str);
char int_to_hex (uint n);

#endif
