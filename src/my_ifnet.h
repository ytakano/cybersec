#ifndef CYBER_MY_IFNET_H
#define CYBER_MY_IFNET_H

#include <netinet/in.h>
#include <stdint.h>
#include <sys/queue.h>

// インターフェース情報を保持する構造体
struct my_ifnet {
    int idx;                       // インデックス
    uint8_t ifaddr[6];             // MACアドレス
    struct in_addr addr;           // IPv4アドレス
    struct in6_addr addr6;         // IPv6アドレス
    uint8_t plen;                  // IPv4プレフィックス長
    uint8_t plen6;                 // IPv6プレフィックス長
    char infile[128];              // 入力UNIXファイル名
    char outfile[128];             // 出力UNIXファイル名
    int infd;                      // 入力先UNIXドメインソケット
    int outfd;                     // 出力先UNIXドメインソケット
    LIST_ENTRY(my_ifnet) pointers; // リスト
};

LIST_HEAD(my_ifnet_head, my_ifnet);

extern struct my_ifnet_head ifs;
extern int numif;
extern void init_my_ifnet();
extern struct my_ifnet *add_if(const char *ipv4, uint8_t plen4,
                               const char *ipv6, uint8_t plen6, const char *in,
                               const char *out);
extern void dev_input(int fd);
extern struct my_ifnet *find_if(int idx);
extern void print_if();

#endif // CYBER_MY_IFNET_H