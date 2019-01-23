#ifndef CYBER_ETHER_H
#define CYBER_ETHER_H

#include "my_ifnet.h"

#include <netinet/if_ether.h>

void ether_input(struct my_ifnet *ifp, struct ether_header *eh);

#endif // CYBER_ETHER_H