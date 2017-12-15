/**
 *author: Vinod Reddy
 *helper functions to create ARP request and Reply frames and Ethernet frames
 *
 *Edit1:
 *author: Anmol Vatsa<anvatsa@cs.utah.edu>
 */

#include "types.h"
#include "util.h"
#include "defs.h"
#include "arp_frame.h"

#define BROADCAST_MAC "FF:FF:FF:FF:FF:FF"

int hex_to_int (char ch) {

	uint i = 0;

	if (ch >= '0' && ch <= '9') {
		i = ch - '0';
	}
	else if (ch >= 'A' && ch <= 'F') {
		i = 10 + (ch - 'A');
	}
	else if (ch >= 'a' && ch <= 'f') {
		i = 10 + (ch - 'a');
	}

	return i;
}

/**
 * Pack the XX:Xx:XX:XX:XX:XX representation of MAC address
 * into I:I:I:I:I:I
 */
void pack_mac(uchar* dest, char* src) {
	for (int i = 0, j = 0; i < 17; i += 3) {
		uint i1 = hex_to_int(src[i]);
		uint i2 = hex_to_int(src[i+1]);
		dest[j++] = (i1<<4) + i2;
	}
}

uint32_t get_ip (char* ip, uint len) {
    uint ipv4  = 0;
    char arr[4];
    int n1 = 0;

     uint ip_vals[4];
     int n2 = 0;

     for (int i =0; i<len; i++) {
        char ch = ip[i];
        if (ch == '.') {
            arr[n1++] = '\0';
            n1 = 0;
            ip_vals[n2++] = atoi(arr);
       	    //cprintf("Check ipval:%d , arr:%s",ip_vals[n2],arr);
	} else {

		arr[n1++] = ch;
	}
    }

        arr[n1++] = '\0';
        n1 = 0;
        ip_vals[n2++] = atoi(arr);
        //cprintf("Final Check ipval:%d , arr:%s",ip_vals[n2],arr);

//	ipv4 = (ip_vals[0]<<24) + (ip_vals[1]<<16) + (ip_vals[2]<<8) + ip_vals[3];
	ipv4 = (ip_vals[3]<<24) + (ip_vals[2]<<16) + (ip_vals[1]<<8) + ip_vals[0];
    return ipv4;
}
uint16_t htons(uint16_t v) {
  return (v >> 8) | (v << 8);
}
uint32_t htonl(uint32_t v) {
  return htons(v >> 16) | (htons((uint16_t) v) << 16);
}

int create_eth_arp_frame(uint8_t* smac, char* ipAddr, struct ethr_hdr *eth) {
	cprintf("Create ARP frame\n");
	char* dmac = BROADCAST_MAC;

	pack_mac(eth->dmac, dmac);
	memmove(eth->smac, smac, 6);

	//ether type = 0x0806 for ARP
	eth->ethr_type = htons(0x0806);

	/** ARP packet filling **/
	eth->hwtype = htons(1);
	eth->protype = htons(0x0800);

	eth->hwsize = 0x06;
	eth->prosize = 0x04;

	//arp request
	eth->opcode = htons(1);

	/** ARP packet internal data filling **/
	memmove(eth->arp_smac, smac, 6);
	pack_mac(eth->arp_dmac, dmac); //this can potentially be igored for the request

	eth->sip = get_ip("192.168.1.1", strlen("192.168.1.1"));

	*(uint32_t*)(&eth->dip) = get_ip(ipAddr, strlen(ipAddr));

	return 0;
}


char int_to_hex (uint n) {

    char ch = '0';

    if (n >= 0 && n <= 9) {
        ch = '0' + n;
    }
    else if (n >= 10 && n <= 15) {
        ch = 'A' + (n - 10);
    }

    return ch;

}
// parse the mac address
void unpack_mac(uint8_t* mac, char* mac_str) {

    int c = 0;

    for (int i = 0; i < 6; i++) {
        uint m = mac[i];

        uint i2 = m & 0x0f;
        uint i1 = (m & 0xf0)>>4;

        mac_str[c++] = int_to_hex(i1);
        mac_str[c++] = int_to_hex(i2);

        mac_str[c++] = ':';
    }

    mac_str[c-1] = '\0';

}

// parse the ip value
void parse_ip (uint ip, char* ip_str) {

    uint v = 255;
    uint ip_vals[4];

    for (int i = 0; i >= 0; i--) {
        ip_vals[i] = ip && v;
        v  = v<<8;
    }

    int c = 0;
    for (int i = 0; i < 4; i++) {
        uint ip1 = ip_vals[i];

        if (ip1 == 0) {
            ip_str[c++] = '0';
            ip_str[c++] = ':';
        }
        else {
            //unsigned int n_digits = 0;
            char arr[3];
            int j = 0;

            while (ip1 > 0) {
                arr[j++] = (ip1 % 10) + '0';
                ip1 /= 10;
            }

            for (j = j-1; j >= 0; j--) {
                ip_str[c++] = arr[j];
            }

            ip_str[c++] = ':';
        }
    }

    ip_str[c-1] = '\0';

}

// ethernet packet arrived; parse and get the MAC address
void parse_arp_reply(struct ethr_hdr eth) {
	if (eth.ethr_type != 0x0806) {
		cprintf("Not an ARP packet");
		return;
	}

	if (eth.protype != 0x0800) {
		cprintf("Not IPV4 protocol\n");
		return;
	}

	if (eth.opcode != 2) {
		cprintf("Not an ARP reply\n");
		return;
	}

	char* my_mac = (char*)"FF:FF:FF:FF:FF:FF";
	char dst_mac[18];

	unpack_mac(eth.arp_dmac, dst_mac);

	if (strcmp((const char*)my_mac, (const char*)dst_mac)) {
		cprintf("Not the intended recipient\n");
		return;
	}

	//parse sip; it should be equal to the one we sent
	char* my_ip = (char*)"255.255.255.255";
	char dst_ip[16];

	parse_ip(eth.dip, dst_ip);

	if (strcmp((const char*)my_ip, (const char*)dst_ip)) {
	    cprintf("Not the intended recipient\n");
	    return;
	}

	char mac[18];
	unpack_mac(eth.arp_smac, mac);

	cprintf((char*)mac);
}
