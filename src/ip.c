#include "ip.h"
#include "flags.h"

#include "ether.h"
#include "poptrie/poptrie.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/if_ether.h>

#define SNDBUFSIZ 256

#define IPHASH(ADDR)                                                           \
    (((uint8_t *)&(ADDR))[0] ^ ((uint8_t *)&(ADDR))[1] ^                       \
     ((uint8_t *)&(ADDR))[2] ^ ((uint8_t *)&(ADDR))[3])

// ARPテーブル定義
struct ip2mac {
    uint8_t macaddr[ETHER_ADDR_LEN];
    struct in_addr ip_addr;
};

static struct poptrie *poptrie; // ルーティングテーブル

struct ip2mac arptable[256]; // MACアドレステーブルのインスタンス

uint8_t *sendbuf[SNDBUFSIZ]; // 送信バッファ
int sendbuf_len = 0;         // 送信バッファを利用中の数

static void forward(struct ip *iph);

static struct ip2mac *find_mac(struct in_addr *addr) {
    uint8_t hash = IPHASH(addr->s_addr);
    if (arptable[hash].ip_addr.s_addr == addr->s_addr)
        return &arptable[hash];

    return NULL;
}

static bool is_to_me(struct ip *iph) {
    for (int i = 0; i < NUMIF; i++) {
        if (memcmp(&interfaces[i].addr, &iph->ip_dst, sizeof(iph->ip_dst)) == 0)
            return true;
    }

    return false;
}

void ipv4_input(struct ip *iph) {
    // 自ホスト宛かチェック
    if (!is_to_me(iph)) {
        if (IS_L3BRIDGE) {
            forward(iph);
        }
        return;
    }

    // tcp_input();
    // udp_input();
}

void route_add(struct in_addr *addr, int plen, struct my_ifnet *ifp) {
    poptrie_route_add(poptrie, ntohl(addr->s_addr), plen, ifp);
}

static struct my_ifnet *route_lookup(struct in_addr *addr) {
    return (struct my_ifnet *)poptrie_lookup(poptrie, ntohl(addr->s_addr));
}

void init_ipv4() {
    poptrie = poptrie_init(NULL, 19, 22);
    if (poptrie == NULL) {
        fprintf(stderr, "failed to initialize poptrie\n");
        exit(1);
    }
}

static void forward(struct ip *iph) {
    // ルーティングテーブルから宛先インターフェースを決定
    struct my_ifnet *ifp = (struct my_ifnet *)route_lookup(&iph->ip_dst);
    if (ifp == NULL) // 宛先がルーティングテーブルに存在しない
        return;

    uint32_t len = ntohs(iph->ip_len);                   // IPパケット長
    uint32_t ethlen = sizeof(struct ether_header) + len; // Ethernetフレーム長

    uint8_t buf[ethlen]; // 一時バッファ
    struct ether_header *eh = (struct ether_header *)buf;

    // Ethernetヘッダおよび、IPヘッダへアドレスを設定
    memcpy(eh->ether_shost, ifp->ifaddr, ETHER_ADDR_LEN);
    memcpy(&eh[1], iph, len);
    eh->ether_type = htons(ETHERTYPE_IP);

    // 宛先IPアドレスの宛先MACアドレスをARPテーブルから検索
    struct ip2mac *mac = find_mac(&iph->ip_dst);
    if (mac == NULL) {
        // ARPテーブルにないため、宛先IPアドレスに対応するMACアドレスを問い合わせ

        // 送信バッファが満杯な場合はパケットを破棄
        if (sendbuf_len >= SNDBUFSIZ)
            return;

        // 送信バッファにコピー
        uint8_t *mbuf = malloc(ethlen);
        memcpy(mbuf, buf, ethlen);
        sendbuf[sendbuf_len] = mbuf;
        sendbuf_len++;
    } else {
        // 宛先MACアドレスを設定
        memcpy(eh->ether_dhost, mac->macaddr, ETHER_ADDR_LEN);

        // インターフェースへ出力
        ether_output(ifp, eh);
    }
}