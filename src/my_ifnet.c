#include "my_ifnet.h"

#include <arpa/inet.h>
#include <stdlib.h>

struct my_ifnet *interfaces = NULL;
int numif = 0;

void init_interfaces(int num) {
    num = num < 10 ? num : 10;

    interfaces = malloc(sizeof(*interfaces));
    numif = num;

    for (int n = 0; n < num; n++) {
        interfaces[n].ifaddr[0] = 0x02;
        interfaces[n].ifaddr[1] = rand() & 0xff;
        interfaces[n].ifaddr[2] = rand() & 0xff;
        interfaces[n].ifaddr[3] = rand() & 0xff;
        interfaces[n].ifaddr[4] = rand() & 0xff;
        interfaces[n].ifaddr[5] = rand() & 0xff;
    }
}