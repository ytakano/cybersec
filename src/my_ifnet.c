#include "my_ifnet.h"

struct my_ifnet interfaces[NUMIF];

void init_interfaces() {
    // 00-00-5E-00-53-00
    interfaces[0].ifaddr[0] = 0x00;
    interfaces[0].ifaddr[1] = 0x00;
    interfaces[0].ifaddr[2] = 0x5e;
    interfaces[0].ifaddr[3] = 0x00;
    interfaces[0].ifaddr[4] = 0x53;
    interfaces[0].ifaddr[5] = 0x00;

    // 00-00-5E-00-53-01
    interfaces[1].ifaddr[0] = 0x00;
    interfaces[1].ifaddr[1] = 0x00;
    interfaces[1].ifaddr[2] = 0x5e;
    interfaces[1].ifaddr[3] = 0x00;
    interfaces[1].ifaddr[4] = 0x53;
    interfaces[1].ifaddr[5] = 0x01;

    // 00-00-5E-00-53-02
    interfaces[2].ifaddr[0] = 0x00;
    interfaces[2].ifaddr[1] = 0x00;
    interfaces[2].ifaddr[2] = 0x5e;
    interfaces[2].ifaddr[3] = 0x00;
    interfaces[2].ifaddr[4] = 0x53;
    interfaces[2].ifaddr[5] = 0x02;
}