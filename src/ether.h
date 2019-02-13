#ifndef CYBER_ETHER_H
#define CYBER_ETHER_H

#include "my_ifnet.h"

#include <netinet/if_ether.h>

void ether_input(struct my_ifnet *ifp, struct ether_header *eh, int len);
void ether_output(struct my_ifnet *ifp, struct ether_header *eh, int len);

#endif // CYBER_ETHER_H