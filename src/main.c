#include "ether.h"
#include "flags.h"
#include "ip.h"
#include "my_ifnet.h"

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
 * IPv4, TCP SYNパケット生成用関数
 * 引数:
 *   buf: IPパケットへのポインタ
 *   ifp: 送信用インターフェース
 *   ipdst: 宛先IPv4アドレス
 *   dport: 宛先ポート番号
 */
void gen_tcpsyn4(void *buf, struct my_ifnet *ifp, struct in_addr *ipdst,
                 uint16_t dport) {
    struct ip *iph = buf;
    struct tcphdr *tcph = (struct tcphdr *)&((uint8_t *)buf)[sizeof(struct ip)];

    iph->ip_v = 4;
    iph->ip_hl = sizeof(struct ip) >> 2;
    iph->ip_tos = 0;
    iph->ip_len = htons(sizeof(struct ip) + sizeof(struct tcphdr));
    iph->ip_id = 0;
    iph->ip_off = 0;
    iph->ip_p = IPPROTO_TCP;
    iph->ip_src = ifp->addr;
    iph->ip_dst = *ipdst;

    tcph->th_dport = htons(dport);
    tcph->th_seq = htonl(rand());
    tcph->th_ack = 0;
    tcph->th_x2 = 0;
    tcph->th_off = sizeof(struct tcphdr) >> 2;
    tcph->th_flags = TH_SYN;
    tcph->th_win = htons(4096);
    tcph->th_urp = 0;

    // TODO: checksum of TCP
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

    gen_tcpsyn4(buf, ifp, &ipdst, dport);

    ipv4_output((struct ip *)buf);
}

/*
 * ブリッジ設定用関数
 */
void set_bridge() {
    char l2[20];
    char l3[20];

    printf("L2 bridge (y/n?): ");
    scanf("%s", l2);

    if (memcmp(l2, "y", 1) == 0) {
        SET_L2BRIDGE(true);
    } else if (memcmp(l2, "n", 1) == 0) {
        SET_L2BRIDGE(false);
    }

    printf("L3 bridge (y/n?): ");
    scanf("%s", l3);

    if (memcmp(l3, "y", 1) == 0) {
        SET_L3BRIDGE(true);
    } else if (memcmp(l3, "n", 1) == 0) {
        SET_L3BRIDGE(false);
    }
}

/*
 * イベントループ
 */
void eventloop() {
    int kq = kqueue();
    add2event(kq, STDIN_FILENO);

    for (;;) {
        printf("command = show | create | tcp | bridge | exit\n> ");
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
            } else if (memcmp("bridge", buf, 4) == 0) {
                set_bridge();
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