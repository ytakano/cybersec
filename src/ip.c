#include "ip.h"
#include "flags.h"

#include "ether.h"
#include "tcp.h"

#include "poptrie/poptrie.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/if_ether.h>

#include <sys/queue.h>

#define SNDBUFSIZ 256

// IPv4アドレスからハッシュ値を計算するマクロ
#define IPHASH(ADDR)                                                           \
    (((uint8_t *)&(ADDR))[0] ^ ((uint8_t *)&(ADDR))[1] ^                       \
     ((uint8_t *)&(ADDR))[2] ^ ((uint8_t *)&(ADDR))[3])

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

// ARPテーブル定義
struct ip2mac {
    uint8_t macaddr[ETHER_ADDR_LEN];
    struct in_addr ip_addr;
};

struct ip2mac arptable[256]; // MACアドレステーブルのインスタンス

/*
 * 送信バッファの用構造体
 */
struct sendbuf {
    struct in_addr nextip;
    struct ether_header *eh;
    uint32_t ethlen;
    LIST_ENTRY(sendbuf) pointers;
};

LIST_HEAD(sendbuf_head, sendbuf) sbuf; // 送信バッファ

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

static void forward(struct ip *iph);

/*
 * ARPテーブルからIPアドレスに対応するエントリを検索
 * 引数:
 *   addr: 検索キーとするアドレスへのポインタ
 * 返り値:
 *   対応するARPテーブルへのポインタ
 *   見つからない場合NULLを返す
 */
static struct ip2mac *find_mac(struct in_addr *addr) {
    uint8_t hash = IPHASH(addr->s_addr);
    if (arptable[hash].ip_addr.s_addr == addr->s_addr)
        return &arptable[hash];

    return NULL;
}

/*
 * ARPテーブルへIPアドレスとMACアドレスの組を追加
 * 引数:
 *   addr: キーとなるIPアドレスへのポインタ
 *   macaddr: 対応するMACアドレスへのポインタ
 */
static void add2arptable(struct in_addr *addr, uint8_t *macaddr) {
    uint8_t hash = IPHASH(addr->s_addr);
    arptable[hash].ip_addr = *addr;
    memcpy(arptable[hash].macaddr, macaddr, ETHER_ADDR_LEN);
}

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
            forward(iph);
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
    forward(iph);
}

/*
 * IPv4ルーティングテーブルへ新規経路を追加する関数
 * ネクストホップアドレスが無い場合はNULLを指定すること
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
static struct rtentry *route_lookup(struct in_addr *addr) {
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
            printf("%s\n", addr);
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

    LIST_INIT(&sbuf);
    LIST_INIT(&fib);
}

/*
 * ARPリクエストを送信
 * 引数:
 *   ifp: 送信を行うインターフェース
 *   addr: 問い合わせを行うIPアドレスへのポインタ
 */
static void arp_req(struct my_ifnet *ifp, struct in_addr *addr) {
    uint8_t buf[ETHER_HDR_LEN + sizeof(struct ether_arp)];
    struct ether_header *eh = (struct ether_header *)buf;
    struct ether_arp *req = (struct ether_arp *)(buf + ETHER_HDR_LEN);

    // Ethenerヘッダ設定
    memcpy(eh->ether_shost, ifp->ifaddr, ETHER_ADDR_LEN); // 送信元MACは自分
    memset(eh->ether_dhost, 0xff, ETHER_ADDR_LEN); // 宛先MACはブロードキャスト
    eh->ether_type = htons(ETHERTYPE_ARP);

    // ARPヘッダ設定
    req->ea_hdr.ar_hrd = ntohs(ARPHRD_ETHER); // ハードウェアタイプ (Ethernet)
    req->ea_hdr.ar_pro = ntohs(ETHERTYPE_IP); // プロトコルタイプ (IP)
    req->ea_hdr.ar_hln = ETHER_ADDR_LEN; // 6バイト。MACアドレスサイズ
    req->ea_hdr.ar_pln = sizeof(struct in_addr); // 4バイト。IPv4アドレスサイズ
    req->ea_hdr.ar_op = ntohs(ARPOP_REQUEST); // ARPリクエスト

    // ARPリクエスト設定
    memcpy(req->arp_sha, ifp->ifaddr, ETHER_ADDR_LEN); // 送信元MACは自分
    memcpy(req->arp_spa, &ifp->addr, sizeof(req->arp_spa)); // 送信元IPは自分
    memset(req->arp_tha, 0xff, ETHER_ADDR_LEN); // 宛先MACはブロードキャスト
    memcpy(req->arp_tpa, addr, sizeof(*addr)); // 質問先IP

    ether_output(ifp, eh, ETHER_HDR_LEN + sizeof(struct ether_arp));
}

/*
 * ARPリクエストを受け取り、ARPリプライを返す関数
 * 引数:
 *   ifp: 受信したインターフェース
 *   arph: ARPリクエストへのポインタ
 */
static void arp_req_input(struct my_ifnet *ifp, struct arphdr *arph) {
    struct ether_arp *req = (struct ether_arp *)arph;
    uint8_t buf[ETHER_HDR_LEN + sizeof(struct ether_arp)];
    struct ether_header *eh = (struct ether_header *)buf;
    struct ether_arp *reply = (struct ether_arp *)(buf + ETHER_HDR_LEN);

    char addr[16];
    inet_ntop(PF_INET, req->arp_tpa, addr, sizeof(addr));
    printf("ARP: who has %s?\n", addr);

    // ARPテーブルに追加
    add2arptable((struct in_addr *)req->arp_spa, req->arp_sha);

    struct in_addr *tpa = (struct in_addr *)req->arp_tpa;
    if (tpa->s_addr != ifp->addr.s_addr)
        return;

    // Ethenerヘッダ設定
    memcpy(eh->ether_shost, ifp->ifaddr, ETHER_ADDR_LEN); // 送信元MACは自分
    memcpy(eh->ether_dhost, req->arp_sha, ETHER_ADDR_LEN); // 宛先MAC
    eh->ether_type = htons(ETHERTYPE_ARP);

    // ARPヘッダ設定
    reply->ea_hdr.ar_hrd = ntohs(ARPHRD_ETHER); // ハードウェアタイプ (Ethernet)
    reply->ea_hdr.ar_pro = ntohs(ETHERTYPE_IP); // プロトコルタイプ (IP)
    reply->ea_hdr.ar_hln = ETHER_ADDR_LEN; // 6バイト。MACアドレスサイズ
    reply->ea_hdr.ar_pln =
        sizeof(struct in_addr); // 4バイト。IPv4アドレスサイズ
    reply->ea_hdr.ar_op = ntohs(ARPOP_REPLY); // ARPリプライ

    // ARPリプライ設定
    memcpy(reply->arp_sha, ifp->ifaddr, ETHER_ADDR_LEN); // 送信元MACは自分
    memcpy(reply->arp_spa, &ifp->addr,
           sizeof(reply->arp_spa)); // 送信元IPは自分
    memcpy(reply->arp_tha, req->arp_sha, ETHER_ADDR_LEN);         // 宛先MAC
    memcpy(reply->arp_tpa, req->arp_spa, sizeof(reply->arp_tpa)); // 質問先IP

    ether_output(ifp, eh, ETHER_HDR_LEN + sizeof(struct ether_arp));
}

/*
 * ARPリプライを受け取り、送信バッファ中のフレームを送信する関数
 * 引数:
 *   ifp: 入力インターフェース
 *   arph:
 */
static void arp_reply_input(struct my_ifnet *ifp, struct arphdr *arph) {
    struct ether_arp *rep = (struct ether_arp *)arph;

    char addr[16];
    inet_ntop(PF_INET, rep->arp_spa, addr, sizeof(addr));
    printf("ARP: %02X-%02X-%02X-%02X-%02X-%02X has %s\n", rep->arp_sha[0],
           rep->arp_sha[1], rep->arp_sha[2], rep->arp_sha[3], rep->arp_sha[4],
           rep->arp_sha[5], addr);

    // ARPテーブルに追加
    add2arptable((struct in_addr *)rep->arp_spa, rep->arp_sha);

    // 送信バッファ中のフレームを送信
    struct sendbuf *np;
    for (np = sbuf.lh_first; np != NULL;) {
        if (np->eh->ether_type == htons(ETHERTYPE_IP)) {
            struct ip2mac *mac = find_mac(&np->nextip);
            if (mac == NULL) {
                np = np->pointers.le_next;
                continue;
            }

            // 宛先MACアドレスを設定
            memcpy(np->eh->ether_dhost, mac->macaddr, ETHER_ADDR_LEN);

            // インターフェースへ出力
            ether_output(ifp, np->eh, np->ethlen);

            // バッファを解放
            free(np->eh);
            struct sendbuf *tmp = np->pointers.le_next;
            LIST_REMOVE(np, pointers);
            free(np);
            np = tmp;
        }
    }
}

/*
 * ARPリクエスト及び応答を受け取る関数
 * 引数:
 *   ifp: 入力インターフェース
 *   arph: 入力ARP
 */
void arp_input(struct my_ifnet *ifp, struct arphdr *arph) {
    // Ethernet以外は未対応
    if (ntohs(arph->ar_hrd) != ARPHRD_ETHER || arph->ar_hln != ETHER_ADDR_LEN)
        return;

    // IP以外は未対応
    if (ntohs(arph->ar_pro) != ETHERTYPE_IP ||
        arph->ar_pln != sizeof(struct in_addr))
        return;

    switch (ntohs(arph->ar_op)) {
    case ARPOP_REQUEST:
        // ARPリクエストを受け取り応答
        arp_req_input(ifp, arph);
        return;
    case ARPOP_REPLY:
        // REPLYを受け取って送信バッファ中のフレームを送信
        arp_reply_input(ifp, arph);
        return;
    default:
        // ARP要求と応答以外は未対応
        return;
    }
}

/*
 * 送信バッファに追加。バッファへは単純にコピーして保存。
 * 引数:
 *   eh: 送信するEthernetフレームへのポインタ
 *   ethlen: フレームサイズ
 *   nextip: 次ホップIPアドレス
 */
static void add2sendbuf(struct ether_header *eh, uint32_t ethlen,
                        struct in_addr *nextip) {
    // 送信バッファへコピー
    struct ether_header *buf = malloc(ethlen);
    memcpy(buf, eh, ethlen);

    // リンクリストのエントリを作成
    struct sendbuf *entry = malloc(sizeof(struct sendbuf));
    entry->eh = buf;
    entry->ethlen = ethlen;
    entry->nextip = *nextip;

    LIST_INSERT_HEAD(&sbuf, entry, pointers); // リンクリストへ追加
}

/*
 * ルーティングテーブルに基づいてIPパケットの転送を行う関数
 * 引数:
 *   iph: 転送するIPパケットへのポインタ
 */
static void forward(struct ip *iph) {
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

/*
 * ARPテーブルを表示する関数
 */
void print_arp() {
    for (int i = 0; i < sizeof(arptable) / sizeof(arptable[0]); i++) {
        if (arptable[i].ip_addr.s_addr != 0) {
            char addr[16];
            inet_ntop(PF_INET, &arptable[i].ip_addr, addr, sizeof(addr));
            printf("%s %02X-%02X-%02X-%02X-%02X-%02X\n", addr,
                   arptable[i].macaddr[0], arptable[i].macaddr[1],
                   arptable[i].macaddr[2], arptable[i].macaddr[3],
                   arptable[i].macaddr[4], arptable[i].macaddr[5]);
        }
    }
}