#include "ether.h"
#include "flags.h"
#include "my_ifnet.h"

int main(int argc, char *argv[]) {
    init_interfaces(3); // インターフェースを初期化
    SET_L2BRIDGE(1);

    return 0;
}