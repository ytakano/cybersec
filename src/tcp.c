#include "tcp.h"
#include "ip.h"

#include <netinet/tcp.h>

#include <stdio.h>
#include <stdlib.h>

void tcp_input(struct tcphdr *tcph) {
    printf("TCP: from %d to %d, flag = ", ntohs(tcph->th_sport),
           ntohs(tcph->th_dport));

    if (tcph->th_flags & TH_SYN) {
        printf("S");
    }

    if (tcph->th_flags & TH_ACK) {
        printf("A");
    }

    if (tcph->th_flags & TH_FIN) {
        printf("F");
    }

    if (tcph->th_flags & TH_PUSH) {
        printf("P");
    }

    if (tcph->th_flags & TH_CWR) {
        printf("U");
    }

    if (tcph->th_flags & TH_CWR) {
        printf("C");
    }

    if (tcph->th_flags & TH_ECE) {
        printf("E");
    }

    printf("\n");
}

/*
 * IPv4, TCP SYNパケット生成用関数
 * 引数:
 *   buf: IPパケットへのポインタ
 *   ifp: 送信用インターフェース
 *   ipdst: 宛先IPv4アドレス
 *   dport: 宛先ポート番号
 */
void tcp_output(void *buf, struct my_ifnet *ifp, struct in_addr *ipdst,
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

    ipv4_output((struct ip *)buf);
}
