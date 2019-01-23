#ifndef CYBER_MY_IFNET_H
#define CYBER_MY_IFNET_H

#include <stdint.h>

#define NUMIF 3

// インターフェース情報を保持する構造体
struct my_ifnet {
    uint8_t ifaddr[6]; // MACアドレス
};

extern struct my_ifnet interfaces[]; // インターフェース情報

void init_interfaces(); // インターフェース情報を初期化

#endif // CYBER_MY_IFNET_H