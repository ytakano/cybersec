#ifndef CYBER_IP_H
#define CYBER_IP_H

#include <netinet/ip.h>

extern void ipv4_input(struct ip *iph);

#endif // CYBER_IP_H