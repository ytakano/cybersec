#include "ether.h"
#include "flags.h"
#include "my_ifnet.h"

// send a frame from 00-00-5e-00-53-10 to ff-ff-ff-ff-ff-ff
void in_ether01() {
    struct ether_header eh;
    eh.ether_dhost[0] = 0xff;
    eh.ether_dhost[1] = 0xff;
    eh.ether_dhost[2] = 0xff;
    eh.ether_dhost[3] = 0xff;
    eh.ether_dhost[4] = 0xff;
    eh.ether_dhost[5] = 0xff;

    eh.ether_shost[0] = 0x00;
    eh.ether_shost[1] = 0x00;
    eh.ether_shost[2] = 0x5e;
    eh.ether_shost[3] = 0x00;
    eh.ether_shost[4] = 0x53;
    eh.ether_shost[5] = 0x10;

    ether_input(&interfaces[0], &eh);
}

// send a frame from 00-00-5e-00-53-11 to 00-00-5e-00-53-10
void in_ether02() {
    struct ether_header eh;
    eh.ether_dhost[0] = 0x00;
    eh.ether_dhost[1] = 0x00;
    eh.ether_dhost[2] = 0x5e;
    eh.ether_dhost[3] = 0x00;
    eh.ether_dhost[4] = 0x53;
    eh.ether_dhost[5] = 0x10;

    eh.ether_shost[0] = 0x00;
    eh.ether_shost[1] = 0x00;
    eh.ether_shost[2] = 0x5e;
    eh.ether_shost[3] = 0x00;
    eh.ether_shost[4] = 0x53;
    eh.ether_shost[5] = 0x11;

    ether_input(&interfaces[1], &eh);
}

int main(int argc, char *argv[]) {
    init_interfaces(); // インターフェースを初期化
    SET_L2BRIDGE(1);

    in_ether01(); // 00-00-5e-00-53-10 -> ff-ff-ff-ff-ff-ff (IF#0)
    in_ether02(); // 00-00-5e-00-53-11 -> 00-00-5e-00-53-10 (IF#1)

    return 0;
}