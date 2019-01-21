#include "flags.h"
#include "ip.h"
#include "ipv6.h"
#include "my_ifnet.h"

#include <netinet/if_ether.h>
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
    // 送信元MACアドレスからハッシュ値を計算
    uint8_t hash = MACHASN(eh->ether_shost);

    // テーブルに追加
    mactable[hash].ifp = ifp;
    memcpy(mactable[hash].addr, eh->ether_shost, ETHER_ADDR_LEN);
}

/*
 * MACアドレステーブルから対応するインターフェースを取得
 * 返り値:
 *   対応するmy_ifnet 構造体へのポインタ。
 *   ただし、テーブルにMACアドレスが存在しない場合はNULLが返る。
 * 引数:
 *   ifp: 入力インターフェース
 *   eh: 入力フレーム
 */
static struct my_ifnet *find_interface(struct ether_header *eh) {
    // 送信先MACアドレスからハッシュ値を計算
    uint8_t hash = MACHASH(eh->ether_dhost);

    // 送信元MACアドレスとMACアドレステーブルのMACアドレスとを比較
    if (memcpy(mactable[hash].addr, eh->ether_dhost, ETHER_ADDR_LEN) != 0)
        return NULL;

    return mactable[hash].ifp;
}

static void bridge_input(struct my_ifnet *ifp, struct ether_header *eh) {
    // ブロードキャストなら全てに送信
    // MACアドレステーブルにない場合も全てに送信
}

/*
 * Ethernetフレーム入力関数
 * 引数:
 *   ifp: 入力インターフェース
 *   eh: 入力フレーム
 */
void ether_input(struct my_ifnet *ifp, struct ether_header *eh) {
    // 宛先MACアドレスが自ホスト宛かチェック
    if (memcmp(ifp->ifaddr, eh->ether_dhost, ETHER_ADDR_LEN) != 0) {
        // L2ブリッジのフラグが立っていればL2ブリッジ処理へ
        if (IS_L2BRIDGE)
            bridge_input(ifp, eh);

        return;
    }

    switch (eh->ether_type) {
    case ETHERTYPE_IP: // IPv4入力
        ipv4_input((struct ip *)&eh[1]);
        break;
    case ETHERTYPE_IPV6: // IPv6入力
        ipv6_input((struct ip6_hdr *)&eh[1]);
        break;
    default:
        printf("eh->ether_type is neither IPv6 nor IPv6\n");
        return;
    }

    return;
}