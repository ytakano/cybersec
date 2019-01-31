#ifndef CYBER_MY_IFNET_H
#define CYBER_MY_IFNET_H

#include <netinet/in.h>
#include <stdint.h>

// インターフェース情報を保持する構造体
struct my_ifnet {
    uint8_t ifaddr[6];     // MACアドレス
    struct in_addr addr;   // IPv4アドレス
    struct in6_addr addr6; // IPv6アドレス
    uint8_t plen;          // IPv4プレフィックス長
    uint8_t plen6;         // IPv6プレフィックス長
};

extern struct my_ifnet *interfaces; // インターフェース情報
extern int numif;

void init_interfaces(int num); // インターフェース情報を初期化

#endif // CYBER_MY_IFNET_H