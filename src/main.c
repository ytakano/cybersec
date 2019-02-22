#include "arp.h"
#include "ether.h"
#include "flags.h"
#include "ip.h"
#include "my_ifnet.h"
#include "tcp.h"

#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * 受信用ディスクリプタ登録用関数
 */
void add2event(int kq, int fd) {
    struct kevent kev;

    EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &kev, 1, NULL, 0, NULL);
}

/*
 * インターフェース生成用関数
 */
struct my_ifnet *create_if() {
    char ipv4[16];
    int plen4;
    char in[101], out[101];

    printf("IPv4 address: ");
    fflush(stdout);
    scanf("%15s", ipv4);

    printf("prefix length of IPv4 address: ");
    fflush(stdout);
    scanf("%d", &plen4);

    printf("%s/%d\n", ipv4, plen4);

    printf("input UNIX socket: ");
    scanf("%100s", in);

    printf("output UNIX socket: ");
    scanf("%100s", out);

    return add_if(ipv4, plen4, NULL, 0, in, out);
}

/*
 * TCP SYNパケット送信用関数
 */
void send_tcp() {
    struct in_addr ipdst;
    struct my_ifnet *ifp;
    char dip[16];
    uint16_t dport;
    int oif;
    char buf[sizeof(struct ip) + sizeof(struct tcphdr)];

    printf("Destination IPv4 address: ");
    fflush(stdout);
    scanf("%15s", dip);

    if (inet_pton(PF_INET, dip, &ipdst) == 0) {
        perror("inet_pton");
        return;
    }

    printf("Destination TCP port: ");
    fflush(stdout);
    scanf("%hu", &dport);

    printf("Interface: ");
    fflush(stdout);
    scanf("%d", &oif);

    ifp = find_if(oif);
    if (ifp == NULL) {
        printf("interface #%d does not exist\n", oif);
        return;
    }

    tcp_output(buf, ifp, &ipdst, dport);
}

/*
 * ブリッジ設定用関数
 */
void set_bridge() {
    char l2[4];
    char l3[4];

    printf("L2 bridge (y/n?): ");
    scanf("%4s", l2);

    if (memcmp(l2, "y", 1) == 0) {
        SET_L2BRIDGE(true);
    } else if (memcmp(l2, "n", 1) == 0) {
        SET_L2BRIDGE(false);
    }

    printf("IPv4 routing (y/n?): ");
    scanf("%4s", l3);

    if (memcmp(l3, "y", 1) == 0) {
        SET_L3BRIDGE(true);
    } else if (memcmp(l3, "n", 1) == 0) {
        SET_L3BRIDGE(false);
    }
}

/*
 * IPv4経路追加コマンド用関数
 */
void route_add_cmd() {
    char nw[16];
    char next[16];
    int plen4;
    int ifnum;

    printf("IPv4 network address: ");
    fflush(stdout);
    scanf("%15s", nw);

    printf("prefix length of IPv4 network address: ");
    fflush(stdout);
    scanf("%d", &plen4);

    printf("next hop IPv4 address: ");
    fflush(stdout);
    scanf("%15s", next);

    struct in_addr in1, in2;
    if (!inet_pton(PF_INET, nw, &in1)) {
        perror("inet_pton");
        return;
    }

    if (!inet_pton(PF_INET, next, &in2)) {
        perror("inet_pton");
        return;
    }

    printf("interface: ");
    fflush(stdout);
    scanf("%d", &ifnum);

    struct my_ifnet *ifp = find_if(ifnum);
    if (ifp == NULL) {
        printf("interface #%d was not found\n", ifnum);
        return;
    }

    route_add(ifp, &in2, &in1, plen4);

    printf("added route to %s/%d via %s (IF #%d)\n\n", nw, plen4, next, ifnum);
}

/*
 * イベントループ
 */
void eventloop() {
    int kq = kqueue();
    add2event(kq, STDIN_FILENO);

    for (;;) {
        printf("command = show | create | tcp | fwdctl | route | exit\n> ");
        fflush(stdout);

        struct kevent kev;
        kevent(kq, NULL, 0, &kev, 1, NULL);

        if (kev.ident == STDIN_FILENO) {
            char buf[4096];
            ssize_t size = read(STDIN_FILENO, buf, sizeof(buf));
            buf[size - 1] = '\0';

            if (memcmp("create", buf, 6) == 0) {
                struct my_ifnet *ifn = create_if();
                if (ifn == NULL) {
                    printf("failed to create an interface\n");
                } else {
                    add2event(kq, ifn->sockfd);
                    printf("interface #%d is successfully created\n\n",
                           ifn->idx);
                }
            } else if (memcmp("show", buf, 4) == 0) {
                printf("interfaces:\n");
                print_if();
                printf("\nroute:\n");
                print_route();
                printf("\narp:\n");
                print_arp();
                printf("\nflags:\n");
                print_flags();
                printf("\n");
            } else if (memcmp("tcp", buf, 4) == 0) {
                send_tcp();
            } else if (memcmp("exit", buf, 4) == 0) {
                exit(0);
            } else if (memcmp("fwdctl", buf, 4) == 0) {
                set_bridge();
            } else if (memcmp("route", buf, 4) == 0) {
                route_add_cmd();
            } else {
                if (strlen(buf) != 0)
                    printf("%s command is not supported\n\n", buf);
            }
        } else {
            dev_input(kev.ident);
        }
    }
}

int main(int argc, char *argv[]) {
    init_my_ifnet();
    init_ipv4();

    eventloop();

    return 0;
}