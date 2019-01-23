#ifndef CYBER_IPV6_H
#define CYBER_IPV6_H

#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/ip6.h>

void ipv6_input(struct ip6_hdr *ip6h);

#endif // CYBER_IPV6_H