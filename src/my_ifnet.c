#include "my_ifnet.h"
#include "ether.h"
#include "ip.h"

#include <sys/socket.h>
#include <sys/un.h>

#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct my_ifnet_head ifs;

int numif = 0;

/*
 * インターフェース用の初期化関数
 */
void init_my_ifnet() {
    srand(time(NULL) + getpid());
    LIST_INIT(&ifs);
}

/*
 * インターフェース番号に対応するmy_ifnet構造体へのポインタを取得する関数
 * 引数:
 *   idx: インターフェース番号
 */
struct my_ifnet *find_if(int idx) {
    for (struct my_ifnet *np = LIST_FIRST(&ifs); np != NULL;
         np = LIST_NEXT(np, pointers)) {
        if (np->idx == idx)
            return np;
    }

    return NULL;
}

/*
 * インターフェースを追加する関数
 * 引数:
 *   ipv4: IPv4アドレス
 *   plen4: IPv4アドレスのプレフィックス長
 *   ipv6: IPv6アドレス
 *   plen6: IPv6アドレスのプレフィックス長
 *   in: 入力用UNIXドメインソケットファイル
 *   out: 出力用UNIXドメインソケットファイル
 */
struct my_ifnet *add_if(const char *ipv4, uint8_t plen4, const char *ipv6,
                        uint8_t plen6, const char *in, const char *out) {
    struct my_ifnet *ptr = malloc(sizeof(struct my_ifnet));

    ptr->idx = numif;
    ptr->plen = plen4;
    ptr->plen6 = plen6;

    if (ipv4 != NULL) {
        if (!inet_pton(PF_INET, ipv4, &ptr->addr)) {
            perror("inet_pton");
            free(ptr);
            return NULL;
        }
    }

    if (ipv6 != NULL) {
        if (!inet_pton(PF_INET6, ipv6, &ptr->addr6)) {
            perror("inet_pton");
            free(ptr);
            return NULL;
        }
    }

    // 入力用ソケット作成
    ptr->sockfd = socket(PF_LOCAL, SOCK_DGRAM, 0);
    unlink(in);

    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = PF_LOCAL;
    sa.sun_len = sizeof(sa);
    strncpy(sa.sun_path, in, sizeof(sa.sun_path));

    bind(ptr->sockfd, (struct sockaddr *)&sa, sizeof(sa));

    memset(&ptr->outun, 0, sizeof(sa));
    ptr->outun.sun_family = PF_LOCAL;
    ptr->outun.sun_len = sizeof(sa);
    strncpy(ptr->outun.sun_path, out, sizeof(ptr->outun.sun_path));

    // ランダムなMACアドレスを生成
    ptr->ifaddr[0] = 0x02;
    ptr->ifaddr[1] = rand() & 0xff;
    ptr->ifaddr[2] = rand() & 0xff;
    ptr->ifaddr[3] = rand() & 0xff;
    ptr->ifaddr[4] = rand() & 0xff;
    ptr->ifaddr[5] = rand() & 0xff;

    //
    size_t size = strlen(in);
    memcpy(ptr->infile, in, size);
    ptr->infile[size] = '\0';

    size = strlen(out);
    memcpy(ptr->outfile, out, size);
    ptr->outfile[size] = '\0';

    // インターフェースのリストへ追加
    LIST_INSERT_HEAD(&ifs, ptr, pointers);

    numif++;

    struct in_addr nextip;
    nextip.s_addr = 0;
    uint32_t mask = ~((1 << (32 - plen4)) - 1);
    uint32_t dst = ntohl(ptr->addr.s_addr) & mask;
    dst = htonl(dst);
    route_add(ptr, &nextip, (struct in_addr *)&dst, plen4);

    return ptr;
}

/*
 * インターフェース入力割り込み関数
 * 引数:
 *   fd: UNIXドメインソケットへのファイルデスクリプタ
 */
void dev_input(int fd) {
    for (struct my_ifnet *np = LIST_FIRST(&ifs); np != NULL;
         np = LIST_NEXT(np, pointers)) {
        if (np->sockfd == fd) {
            char buf[4096];
            ssize_t size;
        again:
            size = recv(fd, buf, sizeof(buf), 0);
            if (size < 0) {
                if ((errno) == EAGAIN)
                    goto again;

                perror("recv");
                break;
            }

            ether_input(np, (struct ether_header *)buf, size);
            break;
        }
    }
}

/*
 * インターフェース情報プリント関数
 */
void print_if() {
    for (struct my_ifnet *np = LIST_FIRST(&ifs); np != NULL;
         np = LIST_NEXT(np, pointers)) {
        char ipv4[16];
        inet_ntop(PF_INET, &np->addr, ipv4, sizeof(ipv4));
        printf("#%d\n", np->idx);
        printf("MAC: %02x-%02x-%02x-%02x-%02x-%02x\n", np->ifaddr[0],
               np->ifaddr[1], np->ifaddr[2], np->ifaddr[3], np->ifaddr[4],
               np->ifaddr[5]);
        printf("IPv4: %s/%d\n", ipv4, np->plen);
        printf("in: %s\n", np->infile);
        printf("out: %s\n", np->outfile);
    }
}