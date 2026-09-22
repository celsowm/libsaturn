#ifndef SATURN_HAL_SMPC_INPUT_LOGIC_HPP
#define SATURN_HAL_SMPC_INPUT_LOGIC_HPP

#include <stdint.h>

#include "saturn/input.h"

namespace saturn::hal::smpc {

constexpr uint8_t kIntbackPort1OnlyMode = 0xCAu;
constexpr uint8_t kIntbackPort2OnlyMode = 0x3Au;

struct DigitalPadSample {
    uint16_t held;
    bool connected;
};

inline uint8_t intback_mode_for_port(uint8_t port) {
    return port == 0u ? kIntbackPort1OnlyMode :
           port == 1u ? kIntbackPort2OnlyMode : 0u;
}

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

inline DigitalPadSample decode_direct_digital_pad(
    uint8_t port_status,
    uint8_t peripheral_id,
    uint8_t d1,
    uint8_t d2
) {
    DigitalPadSample sample{};
    if ((port_status & 0x0Fu) == 0u) return sample;

    const uint8_t peripheral_type = static_cast<uint8_t>(peripheral_id >> 4u);
    const uint8_t data_size = static_cast<uint8_t>(peripheral_id & 0x0Fu);
    if (peripheral_type != 0u || data_size < 2u) return sample;

    sample.connected = true;
    sample.held = translate_standard_pad(d1, d2);
    return sample;
}

}  // namespace saturn::hal::smpc

#endif /* SATURN_HAL_SMPC_INPUT_LOGIC_HPP */
