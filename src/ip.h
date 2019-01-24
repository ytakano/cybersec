#ifndef CYBER_IP_H
#define CYBER_IP_H

#include "my_ifnet.h"

#include <netinet/ip.h>

void ipv4_input(struct ip *iph);
void route_add(struct in_addr *addr, int plen, struct my_ifnet *ifp);
void init_ipv4();

#endif // CYBER_IP_H