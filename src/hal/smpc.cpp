#include "src/hal/smpc.hpp"

#include "saturn/input.h"

namespace saturn::hal::smpc {

namespace {

#define SMPC_IREG0 (*reinterpret_cast<volatile uint8_t*>(0x20100001))
#define SMPC_IREG1 (*reinterpret_cast<volatile uint8_t*>(0x20100003))
#define SMPC_IREG2 (*reinterpret_cast<volatile uint8_t*>(0x20100005))
#define SMPC_OREG0 (*reinterpret_cast<volatile uint8_t*>(0x20100021))
#define SMPC_OREG2 (*reinterpret_cast<volatile uint8_t*>(0x20100025))
#define SMPC_OREG3 (*reinterpret_cast<volatile uint8_t*>(0x20100027))
#define SMPC_OREG31 (*reinterpret_cast<volatile uint8_t*>(0x2010005F))
#define SMPC_SF (*reinterpret_cast<volatile uint8_t*>(0x20100063))
#define SMPC_COMREG (*reinterpret_cast<volatile uint8_t*>(0x2010001F))
#define SMPC_IOSEL1 (*reinterpret_cast<volatile uint8_t*>(0x2010007D))
constexpr uint32_t kSfTimeoutIters = 1000000u;
constexpr uint8_t kIntbackCommand = 0x10u;
constexpr uint8_t kSoundOnCommand = 0x06u;
constexpr uint8_t kSoundOffCommand = 0x07u;

inline void ensure_smpc_control_mode() {
    SMPC_IOSEL1 = 0x00u;
}

inline bool wait_sf_clear() {
    uint32_t spins = 0;
    while ((SMPC_SF & 0x01u) != 0u) {
        if (++spins >= kSfTimeoutIters) {
            return false;
        }
    }
    return true;
}

inline bool issue_simple_command(uint8_t command) {
    if (!wait_sf_clear()) {
        return false;
    }
    SMPC_SF = 0x01u;
    SMPC_COMREG = command;
    if (!wait_sf_clear()) {
        return false;
    }
    return SMPC_OREG31 == command;
}

inline bool intback_read_port1(uint8_t* out_d1, uint8_t* out_d2) {
    if (!wait_sf_clear()) {
        return false;
    }

    SMPC_IREG0 = 0x00u;
    SMPC_IREG1 = 0xCAu;
    SMPC_IREG2 = 0xF0u;
    SMPC_SF = 0x01u;
    SMPC_COMREG = kIntbackCommand;

    if (!wait_sf_clear()) {
        return false;
    }

    const uint8_t port1_status = SMPC_OREG0;
    if ((port1_status & 0x0Fu) == 0u) {
        return false;
    }

    *out_d1 = SMPC_OREG2;
    *out_d2 = SMPC_OREG3;
    return true;
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

}  // namespace

uint16_t read_digital_pad() {
    static bool initialized = false;
    if (!initialized) {
        ensure_smpc_control_mode();
        initialized = true;
    }

    uint8_t d1 = 0u;
    uint8_t d2 = 0u;
    if (!intback_read_port1(&d1, &d2)) {
        return 0u;
    }
    return translate_standard_pad(d1, d2);
}

bool sound_on() {
    ensure_smpc_control_mode();
    return issue_simple_command(kSoundOnCommand);
}

bool sound_off() {
    ensure_smpc_control_mode();
    return issue_simple_command(kSoundOffCommand);
}

}  // namespace saturn::hal::smpc
