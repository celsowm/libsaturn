#include "src/hal/smpc.hpp"

namespace saturn::hal::smpc {

namespace {

volatile uint8_t& SMPC_COMREG = *reinterpret_cast<volatile uint8_t*>(0x20100001);
volatile uint8_t& SMPC_SF = *reinterpret_cast<volatile uint8_t*>(0x20100063);
constexpr uint32_t kSmpcTimeoutSpins = 200000u;

inline volatile uint8_t& oreg(uint32_t n) {
    return *reinterpret_cast<volatile uint8_t*>(0x20100021u + (n * 4u));
}

inline bool wait_smpc_ready() {
    uint32_t spin = 0;
    while ((SMPC_SF & 0x01u) != 0u) {
        ++spin;
        if (spin >= kSmpcTimeoutSpins) {
            return false;
        }
    }
    return true;
}

}  // namespace

uint16_t read_digital_pad() {
    if (!wait_smpc_ready()) {
        return 0u;
    }

    SMPC_COMREG = 0x08;

    if (!wait_smpc_ready()) {
        return 0u;
    }

    const uint8_t hi = oreg(0);
    const uint8_t lo = oreg(1);
    return static_cast<uint16_t>((static_cast<uint16_t>(hi) << 8u) | lo) ^ 0xFFFFu;
}

}  // namespace saturn::hal::smpc
