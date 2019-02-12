#ifndef CYBER_IP_H
#define CYBER_IP_H

#include "my_ifnet.h"

#include <net/if_arp.h>
#include <netinet/ip.h>

void init_ipv4();
void ipv4_input(struct ip *iph);
void ipv4_output(struct ip *iph);
void arp_input(struct my_ifnet *ifp, struct arphdr *arph);
void route_add(struct my_ifnet *ifp, struct in_addr *addr, int plen);

#endif // CYBER_IP_H