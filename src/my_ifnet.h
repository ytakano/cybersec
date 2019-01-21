#ifndef CYBER_MY_IFNET_H
#define CYBER_MY_IFNET_H

#include <stdint.h>

#define NUMIF 3

struct my_ifnet {
    uint8_t ifaddr[6]; // MACアドレス
};

extern struct my_ifnet interfaces[];

#endif // CYBER_MY_IFNET_H