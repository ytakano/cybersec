#ifndef UDP_H
#define UDP_H

#include <netinet/udp.h>

void udp_input(struct udphdr *udph);

#endif // UDP_H