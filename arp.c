#include "types.h"
#include "stat.h"
#include "user.h"

struct arp_ipv4 {
	unsigned char smac[6];
	unsigned int sip;
	unsigned char dmac[6];
	unsigned int dip;
};

struct arp_hdr {
	unsigned int hwtype;
	unsigned int protype;
	unsigned char hwsize;
	unsigned char prosize;
	unsigned int opcode;
	struct arp_ipv4 arp_data;
};


struct ethr_hdr {
	unsigned char dmac[6];
	unsigned char smac[6];
	unsigned char ethr_type[2];
	struct arp_hdr arp;
};


void printf(int fd, char *s, ...)
{
	  write(fd, s, strlen(s));
}

int hex_to_int (unsigned char ch) {
	
	unsigned int i = 0;

	if (ch >= '0' && ch <= '9') {
		i = ch - '0';
	}
	else if (ch >= 'A' && ch <= 'F') {
		i = 10 + (ch - 'A');
	}

	return i;
}


void copy_mac (unsigned char* a, unsigned char* b) {
	
	for (int i = 0, j = 0; i < 17; i += 3) {
		unsigned int i1 = hex_to_int(a[i]);
		unsigned int i2 = hex_to_int(a[i+1]);
		b[j++] = (i1<<4) + i2;
	}
}

unsigned int get_ip (unsigned char* ip, unsigned int len) {

    unsigned int ipv4  = 0;
    
    char arr[4];
    int n1 = 0;
     
     int ip_vals[4];
     int n2 = 0;
     
     for (int i =0; i<len; i++) {
        char ch = ip[i];
        if (ch == '.') {
            arr[n1++] = '\0';
            n1 = 0;
            ip_vals[n2++] = atoi(arr);
        }
        
        arr[n1++] = ch;
    }
    
    ipv4 = (ip_vals[0]<<24) + (ip_vals[1]<<16) + (ip_vals[2]<<8) + ip_vals[3];
    
    return ipv4;
}
     

void create_arp (unsigned char* smac, unsigned char* dmac) {

	printf(1, "Create ARP");
	struct ethr_hdr eth;

	copy_mac(eth.dmac, dmac);
	copy_mac(eth.smac, smac);

	//ether type = 0x0806 for ARP
	eth.ethr_type[0] = 8;
	eth.ethr_type[1] = 6;

	/** ARP packet filling **/
	eth.arp.hwtype = 0x0001;
	eth.arp.protype = 0x0800;

	eth.arp.hwsize = 6;
	eth.arp.prosize = 4;

	//arp request
	eth.arp.opcode = 1;

	/** ARP packet internal data filling **/
	copy_mac(eth.arp.arp_data.smac, smac);
	copy_mac(eth.arp.arp_data.dmac, dmac);

	unsigned char* ip = (unsigned char*)"192.98.60.27";
	eth.arp.arp_data.sip = get_ip(ip, strlen((char*)ip));

	eth.arp.arp_data.dip = 0;
	
	// ehternet packet ready to be sent
}


unsigned char int_to_hex (unsigned int n) {

    unsigned char ch = '0';
    
    if (n >= 0 && n <= 9) {
        ch = '0' + n;
    }
    else if (n >= 10 && n <= 15) {
        ch = 'A' + (n - 10);
    }
    
    return ch;

}
// parse the mac address
void parse_mac (unsigned char* mac, unsigned char* mac_str) {

    int c = 0;
    
    for (int i = 0; i < 6; i++) {
        unsigned int v = 15;
        unsigned int m = mac[i];
        
        unsigned int i2 = m && v;
        unsigned int i1 = m && (v << 15);
        
        mac_str[c++] = int_to_hex(i1);
        mac_str[c++] = int_to_hex(i2);
        
        mac_str[c++] = ':';
    }
    
    mac_str[c-1] = '\0';

}

// parse the ip value
void parse_ip (unsigned int ip, unsigned char* ip_str) {

    unsigned int v = 255;
    unsigned int ip_vals[4];
    
    for (int i = 0; i >= 0; i--) {
        ip_vals[i] = ip && v;
        v  = v<<8;
    }
    
    int c = 0;
    for (int i = 0; i < 4; i++) {
        unsigned int ip1 = ip_vals[i];
        
        if (ip1 == 0) {
            ip_str[c++] = '0';
            ip_str[c++] = ':';
        }
        else {
            //unsigned int n_digits = 0;
            unsigned char arr[3];
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

	int type = (eth.ethr_type[0]<<8) + (eth.ethr_type[1]);

	if (type != 0x0806) {
		printf(1, "Not an ARP packet");
		exit();
	}

	struct arp_hdr arp = eth.arp;

	if (arp.protype != 0x0800) {
		printf(1, "Not IPV4 protocol\n");
		exit();
	}

	if (arp.opcode != 2) {
		printf(1, "Not an ARP reply\n");
		exit();
	}

	unsigned char* my_mac = (unsigned char*)"FF:FF:FF:FF:FF:FF";
	unsigned char dst_mac[18];

	parse_mac(arp.arp_data.dmac, dst_mac);

	if (strcmp((const char*)my_mac, (const char*)dst_mac)) {
		printf(1, "Not the intended recipient\n");
		exit();
	}

	//parse sip; it should be equal to the one we sent
	unsigned char* my_ip = (unsigned char*)"255.255.255.255";
	unsigned char dst_ip[16];
	
	parse_ip(arp.arp_data.dip, dst_ip);
	
	if (strcmp((const char*)my_ip, (const char*)dst_ip)) {
	    printf(1, "Not the intended recipient\n");
	    exit();
	}
	
	unsigned char mac[18];
	parse_mac(arp.arp_data.smac, mac);

	printf(1, (char*)mac);
}
