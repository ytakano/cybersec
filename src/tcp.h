#ifndef TCP_H
#define TCP_H

#include "my_ifnet.h"

#include <netinet/tcp.h>

void tcp_input(struct tcphdr *tcph);
void tcp_output(void *buf, struct my_ifnet *ifp, struct in_addr *ipdst,
                uint16_t dport);

#endif // TCP_H