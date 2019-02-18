#ifndef CYBER_FLAGS_H
#define CYBER_FLAGS_H

#include <stdint.h>

#define FLAG_L2BRIDGE 1
#define FLAG_L3BRIDGE 2

extern uint8_t bridge_flags;

#define IS_L2BRIDGE (FLAG_L2BRIDGE & bridge_flags)
#define IS_L3BRIDGE (FLAG_L3BRIDGE & bridge_flags)
#define SET_L2BRIDGE(FLAG)                                                     \
    do {                                                                       \
        if (FLAG)                                                              \
            bridge_flags |= FLAG_L2BRIDGE;                                     \
        else                                                                   \
            bridge_flags &= ~FLAG_L2BRIDGE;                                    \
    } while (0)
#define SET_L3BRIDGE(FLAG)                                                     \
    do {                                                                       \
        if (FLAG)                                                              \
            bridge_flags |= FLAG_L3BRIDGE;                                     \
        else                                                                   \
            bridge_flags &= ~FLAG_L3BRIDGE;                                    \
    } while (0)

void print_flags();

#endif // CYBER_FLAGS_Hb