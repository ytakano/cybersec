#include "my_ifnet.h"
#include "ether.h"

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

void init_my_ifnet() {
    srand(time(NULL) + getpid());
    LIST_INIT(&ifs);
}

void add_if(const char *ipv4, uint8_t plen4, const char *ipv6, uint8_t plen6,
            const char *in, const char *out) {
    struct my_ifnet *ptr = malloc(sizeof(struct my_ifnet));

    ptr->idx = numif;
    ptr->plen = plen4;
    ptr->plen6 = plen6;

    if (ipv4 != NULL) {
        if (!inet_pton(PF_INET, ipv4, &ptr->addr)) {
            perror("inet_pton");
            free(ptr);
            return;
        }
    }

    if (ipv6 != NULL) {
        if (!inet_pton(PF_INET6, ipv6, &ptr->addr6)) {
            perror("inet_pton");
            free(ptr);
            return;
        }
    }

    // 入力用ソケット作成
    ptr->infd = socket(PF_LOCAL, SOCK_DGRAM, 0);
    unlink(in);

    struct sockaddr_un sa;
    memset(&sa, 0, sizeof(sa));
    sa.sun_family = PF_LOCAL;
    sa.sun_len = sizeof(sa);
    strcpy(sa.sun_path, in);

    bind(ptr->infd, (struct sockaddr *)&sa, sizeof(sa));

    // 出力用ソケット作成
    ptr->outfd = socket(PF_LOCAL, SOCK_DGRAM, 0);
    unlink(out);

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = PF_LOCAL;
    sa.sun_len = sizeof(sa);
    strcpy(sa.sun_path, out);

    bind(ptr->outfd, (struct sockaddr *)&sa, sizeof(sa));

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
}

void dev_input(int fd) {
    for (struct my_ifnet *np = LIST_FIRST(&ifs); np != NULL;
         np = LIST_NEXT(np, pointers)) {
        if (np->infd == fd) {
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

            ether_input(np, (struct ether_header *)buf);
            break;
        }
    }
}

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