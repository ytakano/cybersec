#ifndef CYBER_IP_H
#define CYBER_IP_H

#include "my_ifnet.h"

#include <net/if_arp.h>
#include <netinet/ip.h>

struct rtentry {
    struct my_ifnet *ifp;
    struct in_addr addr;
};

void init_ipv4();
void ipv4_input(struct ip *iph);
void ipv4_output(struct ip *iph);
void arp_input(struct my_ifnet *ifp, struct arphdr *arph);
void route_add(struct my_ifnet *ifp, struct in_addr *next, struct in_addr *addr,
               int plen);
void print_route();

#endif // CYBER_IP_H