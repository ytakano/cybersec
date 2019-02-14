#include "tcp.h"

#include <stdio.h>

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