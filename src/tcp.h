#ifndef TCP_H
#define TCP_H

#include <netinet/tcp.h>

void tcp_input(struct tcphdr *tcph);

#endif // TCP_H