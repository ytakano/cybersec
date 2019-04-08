#include "ip.h"
#include "arp.h"
#include "ether.h"
#include "flags.h"
#include "tcp.h"

#include "poptrie/poptrie.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/if_ether.h>

#define SNDBUFSIZ 256

/*
 * チェックサムを計算する関数
 * 引数:
 *   buf: バッファ
 *   size: バッファのサイズ
 */
uint16_t cksum(void *buf, int size) {
    uint16_t *ptr = (uint16_t *)buf;
    uint32_t sum = 0;

    while (size > 1) {
        sum += ntohs(*ptr);
        size -= 2;

        ptr++;
    }

    if (size > 0)
        sum += *(uint8_t *)ptr;

    sum = (sum & 0xffff) + (sum >> 16); // キャリーを加算
    sum = (sum & 0xffff) + (sum >> 16); // さらにキャリーを加算

    return htons(~sum);
}

static struct poptrie *poptrie; // ルーティングテーブル

/*
 * IPv4ルーティングテーブル用構造体
 */
struct fibentry {
    struct my_ifnet *ifp;
    struct in_addr nextip;
    struct in_addr dstip;
    int plen;
    LIST_ENTRY(fibentry) pointers;
};

LIST_HEAD(fib_head, fibentry) fib; // FIB

static void ipv4_forward(struct ip *iph);

/*
 * パケットが自分宛てか判定する関数
 * 引数:
 *   iph: 入力パケット
 */
static bool is_to_me(struct ip *iph) {
    for (struct my_ifnet *np = LIST_FIRST(&ifs); np != NULL;
         np = LIST_NEXT(np, pointers)) {
        if (memcmp(&np->addr, &iph->ip_dst, sizeof(iph->ip_dst)) == 0)
            return true;
    }

    return false;
}

/*
 * IPv4パケット入力関数
 * 引数:
 *   iph: 入力パケット
 */
void ipv4_input(struct ip *iph) {
    // 自ホスト宛かチェック
    if (!is_to_me(iph)) {
        if (IS_L3BRIDGE) { // L3ブリッジが有効かチェック
            // TTLが1以下なら転送しない
            if (iph->ip_ttl <= 1)
                return;

            // 自ホスト宛でない場合転送
            ipv4_forward(iph);
        }
        return;
    }

    uint8_t *nxt = (uint8_t *)iph;
    nxt += iph->ip_hl * 4;

    switch (iph->ip_p) {
    case IPPROTO_TCP:
        tcp_input((struct tcphdr *)nxt);
        break;
    case IPPROTO_UDP:
        udp_input((struct udphdr *)nxt);
        break;
    default:;
    }
}

/*
 * IPv4出力関数
 * 引数:
 *   iph: 出力パケット
 */
void ipv4_output(struct ip *iph) {
    iph->ip_ttl = 32;
    ipv4_forward(iph);
}

/*
 * IPv4ルーティングテーブルへ新規経路を追加する関数
 * ネクストホップアドレスが無い場合はnextにNULLを指定すること
 * 引数:
 *   ifp: 出力先インターフェース
 *   next: ネクストホップアドレス
 *   addr: 宛先ネットワークアドレス
 *   plen: 宛先ネットワークアドレスのプレフィックス長
 */
void route_add(struct my_ifnet *ifp, struct in_addr *next, struct in_addr *addr,
               int plen) {
    struct rtentry *entry = malloc(sizeof(struct rtentry));
    entry->addr = *next;
    entry->ifp = ifp;

    poptrie_route_add(poptrie, ntohl(addr->s_addr), plen, entry);

    struct fibentry *fibe = malloc(sizeof(struct fibentry));
    fibe->ifp = ifp;
    fibe->nextip = *next;
    fibe->dstip = *addr;
    fibe->plen = plen;

    LIST_INSERT_HEAD(&fib, fibe, pointers);
}

/*
 * IPv4ルーティングテーブルから経路を取得する関数
 * 引数:
 *   addr: キーとなるIPv4アドレス
 */
struct rtentry *route_lookup(struct in_addr *addr) {
    return (struct rtentry *)poptrie_lookup(poptrie, ntohl(addr->s_addr));
}

/*
 * IPv4ルーティングテーブルをプリントする関数
 */
void print_route() {
    char addr[16];
    struct fibentry *fb;
    for (fb = LIST_FIRST(&fib); fb != NULL; fb = LIST_NEXT(fb, pointers)) {
        inet_ntop(PF_INET, &fb->dstip, addr, sizeof(addr));
        printf("%s/%d ", addr, fb->plen);
        if (fb->nextip.s_addr == 0) {
            printf("#%d\n", fb->ifp->idx);
        } else {
            inet_ntop(PF_INET, &fb->nextip, addr, sizeof(addr));
            printf("%s #%d\n", addr, fb->ifp->idx);
        }
    }
}

/*
 * IPv4初期化関数
 */
void init_ipv4() {
    poptrie = poptrie_init(NULL, 19, 22);
    if (poptrie == NULL) {
        fprintf(stderr, "failed to initialize poptrie\n");
        exit(1);
    }

    LIST_INIT(&fib);
}

/*
 * ルーティングテーブルに基づいてIPパケットの転送を行う関数
 * 引数:
 *   iph: 転送するIPパケットへのポインタ
 */
static void ipv4_forward(struct ip *iph) {
    // ルーティングテーブルから宛先インターフェースを決定
    struct rtentry *entry = route_lookup(&iph->ip_dst);
    if (entry == NULL) // 宛先がルーティングテーブルに存在しない
        return;

    uint32_t len = ntohs(iph->ip_len);     // IPパケット長
    uint32_t ethlen = ETHER_HDR_LEN + len; // Ethernetフレーム長

    if (entry->ifp != NULL && entry->ifp->addr.s_addr == iph->ip_dst.s_addr) {
        ipv4_input(iph);
        return;
    }

    iph->ip_ttl--; // TTLを1減算

    // チェックサムを更新
    iph->ip_sum = 0;
    iph->ip_sum = cksum(iph, iph->ip_hl * 4);

    uint8_t buf[ethlen]; // 一時バッファ
    struct ether_header *eh = (struct ether_header *)buf;

    // Ethernetヘッダおよび、IPヘッダへアドレスを設定
    memcpy(eh->ether_shost, entry->ifp->ifaddr,
           ETHER_ADDR_LEN);                // 宛先MACアドレス
    memcpy(buf + ETHER_HDR_LEN, iph, len); // IPヘッダ
    eh->ether_type = htons(ETHERTYPE_IP);  // EthernetタイプをIPに設定

    // 次ホップIPアドレスを決定
    struct in_addr nextip =
        (entry->addr.s_addr == 0) ? iph->ip_dst : entry->addr;

    // 宛先IPアドレスの宛先MACアドレスをARPテーブルから検索
    struct ip2mac *mac = find_mac(&nextip);
    if (mac == NULL) {
        // ARPテーブルにないため、宛先IPアドレスに対応するMACアドレスを問い合わせ
        arp_req(entry->ifp, &nextip);

        // 送信バッファにコピー
        add2sendbuf(eh, ethlen, &nextip);
    } else {
        // 宛先MACアドレスを設定
        memcpy(eh->ether_dhost, mac->macaddr, ETHER_ADDR_LEN);

        // インターフェースへ出力
        ether_output(entry->ifp, eh, ethlen);
    }
}
