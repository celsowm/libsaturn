#include "src/hal/smpc.hpp"

#include "saturn/input.h"

namespace saturn::hal::smpc {

namespace {

volatile uint8_t& SMPC_PDR1 = *reinterpret_cast<volatile uint8_t*>(0x20100075);
volatile uint8_t& SMPC_PDR2 = *reinterpret_cast<volatile uint8_t*>(0x20100077);
volatile uint8_t& SMPC_DDR1 = *reinterpret_cast<volatile uint8_t*>(0x20100079);
volatile uint8_t& SMPC_DDR2 = *reinterpret_cast<volatile uint8_t*>(0x2010007B);
volatile uint8_t& SMPC_IOSEL1 = *reinterpret_cast<volatile uint8_t*>(0x2010007D);
volatile uint8_t& SMPC_EXLE1 = *reinterpret_cast<volatile uint8_t*>(0x2010007F);

constexpr uint32_t kInputSpinWait = 16u;

inline void spin_wait() {
    for (uint32_t i = 0; i < kInputSpinWait; ++i) {
        __asm__ volatile("nop");
    }
}

inline void init_controller_ports() {
    // Match JoEngine's direct pad setup.
    SMPC_DDR1 = 0x60;
    SMPC_DDR2 = 0x60;
    SMPC_IOSEL1 = 0x03;
    SMPC_EXLE1 = 0x00;
}

inline uint16_t read_port1_raw() {
    uint16_t temp = 0u;

    SMPC_PDR1 = 0x60;
    spin_wait();
    temp = static_cast<uint16_t>((SMPC_PDR1 & 0x08u) << 12u);

    SMPC_PDR1 = 0x40;
    spin_wait();
    temp |= static_cast<uint16_t>((SMPC_PDR1 & 0x0Fu) << 8u);

    SMPC_PDR1 = 0x20;
    spin_wait();
    temp |= static_cast<uint16_t>((SMPC_PDR1 & 0x0Fu) << 4u);

    SMPC_PDR1 = 0x00;
    spin_wait();
    temp |= static_cast<uint16_t>((SMPC_PDR1 & 0x0Fu) << 0u);

    return static_cast<uint16_t>(temp ^ 0x8FFFu);
}

inline uint16_t translate_joengine_pad(uint16_t raw) {
    uint16_t held = 0u;

    if ((raw & (1u << 7)) != 0u) held |= SAT_PAD_RIGHT;
    if ((raw & (1u << 6)) != 0u) held |= SAT_PAD_LEFT;
    if ((raw & (1u << 5)) != 0u) held |= SAT_PAD_DOWN;
    if ((raw & (1u << 4)) != 0u) held |= SAT_PAD_UP;
    if ((raw & (1u << 11)) != 0u) held |= SAT_PAD_START;
    if ((raw & (1u << 10)) != 0u) held |= SAT_PAD_A;
    if ((raw & (1u << 8)) != 0u) held |= SAT_PAD_B;
    if ((raw & (1u << 9)) != 0u) held |= SAT_PAD_C;
    if ((raw & (1u << 2)) != 0u) held |= SAT_PAD_X;
    if ((raw & (1u << 1)) != 0u) held |= SAT_PAD_Y;
    if ((raw & (1u << 0)) != 0u) held |= SAT_PAD_Z;
    if ((raw & (1u << 15)) != 0u) held |= SAT_PAD_L;
    if ((raw & (1u << 3)) != 0u) held |= SAT_PAD_R;

    return held;
}

}  // namespace

uint16_t read_digital_pad() {
    static bool initialized = false;
    if (!initialized) {
        init_controller_ports();
        initialized = true;
    }

    const uint16_t raw = read_port1_raw();
    return translate_joengine_pad(raw);
}

}  // namespace saturn::hal::smpc
