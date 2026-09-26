#ifndef SATURN_HAL_SMPC_INPUT_LOGIC_HPP
#define SATURN_HAL_SMPC_INPUT_LOGIC_HPP

#include <stdint.h>

#include "saturn/input.h"

namespace saturn::hal::smpc {

inline uint16_t translate_standard_pad(uint8_t d1, uint8_t d2) {
    uint16_t held = 0u;
    if ((d1 & 0x80u) == 0u) held |= SAT_PAD_RIGHT;
    if ((d1 & 0x40u) == 0u) held |= SAT_PAD_LEFT;
    if ((d1 & 0x20u) == 0u) held |= SAT_PAD_DOWN;
    if ((d1 & 0x10u) == 0u) held |= SAT_PAD_UP;
    if ((d1 & 0x08u) == 0u) held |= SAT_PAD_START;
    if ((d1 & 0x04u) == 0u) held |= SAT_PAD_A;
    if ((d1 & 0x02u) == 0u) held |= SAT_PAD_C;
    if ((d1 & 0x01u) == 0u) held |= SAT_PAD_B;
    if ((d2 & 0x80u) == 0u) held |= SAT_PAD_R;
    if ((d2 & 0x40u) == 0u) held |= SAT_PAD_X;
    if ((d2 & 0x20u) == 0u) held |= SAT_PAD_Y;
    if ((d2 & 0x10u) == 0u) held |= SAT_PAD_Z;
    if ((d2 & 0x08u) == 0u) held |= SAT_PAD_L;
    return held;
}

}  // namespace saturn::hal::smpc

#endif /* SATURN_HAL_SMPC_INPUT_LOGIC_HPP */
