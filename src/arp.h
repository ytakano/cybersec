#ifndef CYBER_ARP_C
#define CYBER_ARP_C

#include "my_ifnet.h"

#include <net/if_arp.h>
#include <netinet/if_ether.h>

// ARPテーブル定義
struct ip2mac {
    uint8_t macaddr[ETHER_ADDR_LEN];
    struct in_addr ip_addr;
};

void arp_input(struct my_ifnet *ifp, struct arphdr *arph);
void arp_init();
void add2sendbuf(struct ether_header *eh, uint32_t ethlen,
                 struct in_addr *nextip);
void arp_req(struct my_ifnet *ifp, struct in_addr *addr);
void print_arp();
struct ip2mac *find_mac(struct in_addr *addr);

#endif // CYBER_ARP_C