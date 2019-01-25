#include "ip.h"
#include "flags.h"

#include "ether.h"
#include "poptrie/poptrie.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/if_ether.h>

#include <sys/queue.h>

#define SNDBUFSIZ 256

#define IPHASH(ADDR)                                                           \
    (((uint8_t *)&(ADDR))[0] ^ ((uint8_t *)&(ADDR))[1] ^                       \
     ((uint8_t *)&(ADDR))[2] ^ ((uint8_t *)&(ADDR))[3])

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

struct sendbuf {
    struct ether_header *eh;
    LIST_ENTRY(sendbuf) pointers;
};

LIST_HEAD(sendbuf_head, sendbuf) sbuf; // 送信バッファ

static struct poptrie *poptrie; // ルーティングテーブル

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
        if (IS_L3BRIDGE) { // L3ブリッジが有効かチェック
            // TTLが1以下なら転送しない
            if (iph->ip_ttl <= 1)
                return;

            // 自ホスト宛でない場合転送
            forward(iph);
        }
        return;
    }

    switch (iph->ip_p) {
    case IPPROTO_TCP:
    case IPPROTO_UDP:
    default:;
    }
}

void route_add(struct my_ifnet *ifp, struct in_addr *addr, int plen) {
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

    ether_output(ifp, eh);
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
    reply->ea_hdr.ar_op = ntohs(ARPOP_REQUEST); // ARPリクエスト

    // ARPリプライ設定
    memcpy(reply->arp_sha, ifp->ifaddr, ETHER_ADDR_LEN); // 送信元MACは自分
    memcpy(reply->arp_spa, &ifp->addr,
           sizeof(reply->arp_spa)); // 送信元IPは自分
    memcpy(reply->arp_tha, req->arp_sha, ETHER_ADDR_LEN);         // 宛先MAC
    memcpy(reply->arp_tpa, req->arp_spa, sizeof(reply->arp_tpa)); // 質問先IP

    ether_output(ifp, eh);
}

/*
 *
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
        arp_req_input(ifp, arph);
        return;
    case ARPOP_REPLY:
        // TODO
        // REPLYを受け取って、ARPテーブルに追加して送信バッファを送信
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
 */
static void add2sendbuf(struct ether_header *eh, uint32_t ethlen) {
    // 送信バッファへコピー
    struct ether_header *buf = malloc(ethlen);
    memcpy(buf, eh, ethlen);

    // リンクリストのエントリを作成
    struct sendbuf *entry = malloc(sizeof(struct sendbuf));
    entry->eh = buf;

    LIST_INSERT_HEAD(&sbuf, entry, pointers); // リンクリストへ追加
}

/*
 * ルーティングテーブルに基づいてIPパケットの転送を行う関数
 * 引数:
 *   iph: 転送するIPパケットへのポインタ
 */
static void forward(struct ip *iph) {
    // ルーティングテーブルから宛先インターフェースを決定
    struct my_ifnet *ifp = (struct my_ifnet *)route_lookup(&iph->ip_dst);
    if (ifp == NULL) // 宛先がルーティングテーブルに存在しない
        return;

    uint32_t len = ntohs(iph->ip_len);     // IPパケット長
    uint32_t ethlen = ETHER_HDR_LEN + len; // Ethernetフレーム長

    iph->ip_ttl--; // TTLを1減算

    // チェックサムを更新
    iph->ip_sum = 0;
    iph->ip_sum = cksum(iph, iph->ip_hl * 4);

    uint8_t buf[ethlen]; // 一時バッファ
    struct ether_header *eh = (struct ether_header *)buf;

    // Ethernetヘッダおよび、IPヘッダへアドレスを設定
    memcpy(eh->ether_shost, ifp->ifaddr, ETHER_ADDR_LEN); // 宛先MACアドレス
    memcpy(buf + ETHER_HDR_LEN, iph, len);                // IPヘッダ
    eh->ether_type = htons(ETHERTYPE_IP); // EthernetタイプをIPに設定

    // 宛先IPアドレスの宛先MACアドレスをARPテーブルから検索
    struct ip2mac *mac = find_mac(&iph->ip_dst);
    if (mac == NULL) {
        // ARPテーブルにないため、宛先IPアドレスに対応するMACアドレスを問い合わせ
        arp_req(ifp, &iph->ip_dst);

        // 送信バッファにコピー
        add2sendbuf(eh, ethlen);
    } else {
        // 宛先MACアドレスを設定
        memcpy(eh->ether_dhost, mac->macaddr, ETHER_ADDR_LEN);

        // インターフェースへ出力
        ether_output(ifp, eh);
    }
}