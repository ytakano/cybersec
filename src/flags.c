#include "flags.h"

#include <stdio.h>

uint8_t bridge_flags = 0;

void print_flags() {
    printf("    L2 bridge: ");
    if (IS_L2BRIDGE)
        printf("true\n");
    else
        printf("false\n");

    printf("    IPv4 routing: ");
    if (IS_L3BRIDGE)
        printf("true\n");
    else
        printf("false\n");
}