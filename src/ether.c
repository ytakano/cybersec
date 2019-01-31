#include "ether.h"
#include "flags.h"
#include "ip.h"
#include "ipv6.h"

#include <stdio.h>
#include <string.h>

#define MACHASH(ADDR)                                                          \
    ((ADDR)[0] ^ (ADDR)[1] ^ (ADDR)[2] ^ (ADDR)[3] ^ (ADDR)[4] ^ (ADDR)[5])
#define IS_BROADCAST(ADDR)                                                     \
    (((ADDR)[0] == 0xFF) && ((ADDR)[1] == 0xFF) && ((ADDR)[2] == 0xFF) &&      \
     ((ADDR)[3] == 0xFF) && ((ADDR)[4] == 0xFF) && ((ADDR)[5] == 0xFF))

// MACアドレステーブル定義
struct mac2if {
    uint8_t addr[ETHER_ADDR_LEN];
    struct my_ifnet *ifp;
};

struct mac2if mactable[256]; // MACアドレステーブルのインスタンス

/*
 * MACアドレステーブルにMACアドレスを追加する関数
 * 引数:
 *   ifp: 入力インターフェース
 *   eh: 入力フレーム
 */
static void add2mactable(struct my_ifnet *ifp, struct ether_header *eh) {
    // 送信元MACアドレスがブロードキャストアドレスならテーブルに追加しない
    if (IS_BROADCAST(eh->ether_shost))
        return;

    // 送信元MACアドレスからハッシュ値を計算
    uint8_t hash = MACHASH(eh->ether_shost);

    // テーブルに追加
    mactable[hash].ifp = ifp;
    memcpy(mactable[hash].addr, eh->ether_shost, ETHER_ADDR_LEN);
}

/*
 * MACアドレステーブルから対応するインターフェースを取得する関数
 * 返り値:
 *   対応するmy_ifnet構造体へのポインタ。
 *   ただし、テーブルにMACアドレスが存在しない場合はNULLが返る。
 * 引数:
 *   eh: 入力Ethernetフレーム
 */
static struct my_ifnet *find_interface(struct ether_header *eh) {
    // 宛先MACアドレスからハッシュ値を計算
    uint8_t hash = MACHASH(eh->ether_dhost);

    // 送信元MACアドレスとMACアドレステーブルのMACアドレスとを比較
    if (memcmp(mactable[hash].addr, eh->ether_dhost, ETHER_ADDR_LEN) != 0)
        return NULL;

    return mactable[hash].ifp;
}

/*
 * ブリッジ処理を行う関数
 * 引数:
 *   ifp: 入力インターフェース
 *   eh: 入力Ethernetフレーム
 */
static void bridge_input(struct my_ifnet *ifp, struct ether_header *eh) {
    // MACアドレステーブルを検索
    struct my_ifnet *outif = find_interface(eh);

    if (outif) {
        // MACアドレステーブルにキャッシュされていた場合、そのインターフェースへ送信
        ether_output(outif, eh);
    } else {
        // MACアドレステーブルにない場合、受信インターフェース以外の全てのインターフェースへ送信
        for (int i = 0; i < numif; i++) {
            if (ifp != &interfaces[i])
                ether_output(&interfaces[i], eh);
        }
    }

    // MACアドレステーブルへキャッシュ
    add2mactable(ifp, eh);
}

/*
 * Ethernetフレーム入力関数
 * 引数:
 *   ifp: 入力インターフェース
 *   eh: 入力フレーム
 */
void ether_input(struct my_ifnet *ifp, struct ether_header *eh) {
    // 宛先MACアドレスが自インターフェース宛かチェック
    if (memcmp(ifp->ifaddr, eh->ether_dhost, ETHER_ADDR_LEN) != 0 &&
        !IS_BROADCAST(eh->ether_dhost)) {
        // 自インターフェース宛でない場合、L2ブリッジのフラグが立っていればL2ブリッジ処理へ
        if (IS_L2BRIDGE)
            bridge_input(ifp, eh);

        return;
    }

    switch (ntohs(eh->ether_type)) {
    case ETHERTYPE_IP: // IPv4入力
        ipv4_input((struct ip *)((uint8_t *)eh + ETHER_HDR_LEN));
        break;
    case ETHERTYPE_IPV6: // IPv6入力
        ipv6_input((struct ip6_hdr *)((uint8_t *)eh + ETHER_HDR_LEN));
        break;
    case ETHERTYPE_ARP: // ARP入力
        arp_input(ifp, (struct arphdr *)((uint8_t *)eh + ETHER_HDR_LEN));
        break;
    default:
        printf("eh->ether_type is neither IPv6 nor IPv6\n");
        return;
    }

    return;
}

void ether_output(struct my_ifnet *ifp, struct ether_header *eh) {
    printf("ether_output:\n");
    printf("    IF#: %d\n", (int)(ifp - interfaces));
    printf("    SRC MAC: %02X-%02X-%02X-%02X-%02X-%02X\n", eh->ether_shost[0],
           eh->ether_shost[1], eh->ether_shost[2], eh->ether_shost[3],
           eh->ether_shost[4], eh->ether_shost[5]);
    printf("    DST MAC: %02X-%02X-%02X-%02X-%02X-%02X\n", eh->ether_dhost[0],
           eh->ether_dhost[1], eh->ether_dhost[2], eh->ether_dhost[3],
           eh->ether_dhost[4], eh->ether_dhost[5]);

    printf("\n");
}