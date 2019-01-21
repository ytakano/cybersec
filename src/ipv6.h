#ifndef CYBER_IPV6_H
#define CYBER_IPV6_H

#include <netinet/ip6.h>

extern void ipv6_input(struct ip6_hdr *ip6h);

#endif // CYBER_IPV6_H