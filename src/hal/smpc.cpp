#include "src/hal/smpc.hpp"

#include "saturn/input.h"

namespace saturn::hal::smpc {

namespace {

volatile uint8_t& SMPC_IREG0 = *reinterpret_cast<volatile uint8_t*>(0x20100001);
volatile uint8_t& SMPC_IREG1 = *reinterpret_cast<volatile uint8_t*>(0x20100003);
volatile uint8_t& SMPC_IREG2 = *reinterpret_cast<volatile uint8_t*>(0x20100005);
volatile uint8_t& SMPC_OREG0 = *reinterpret_cast<volatile uint8_t*>(0x20100021);
volatile uint8_t& SMPC_OREG2 = *reinterpret_cast<volatile uint8_t*>(0x20100025);
volatile uint8_t& SMPC_OREG3 = *reinterpret_cast<volatile uint8_t*>(0x20100027);
volatile uint8_t& SMPC_SF = *reinterpret_cast<volatile uint8_t*>(0x20100063);
volatile uint8_t& SMPC_COMREG = *reinterpret_cast<volatile uint8_t*>(0x2010001F);
volatile uint8_t& SMPC_IOSEL1 = *reinterpret_cast<volatile uint8_t*>(0x2010007D);

constexpr uint32_t kSfTimeoutIters = 1000000u;
constexpr uint8_t kIntbackCommand = 0x10u;

// SMPC control mode (IOSEL=0) is required to read the pad via INTBACK. SH-2
// direct mode (bit-banging PDR1) is reserved for peripherals that need it,
// like the Virtua Gun — the hardware manual states plainly that it must not
// be used otherwise (docs/sega_saturn_hardware/hard/smpc/hon/p01_20.md),
// and the standard digital pad is not one of the exceptions.
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

// Issues INTBACK to fetch port 1's peripheral data only (port 2 left in
// 0-byte mode since nothing here reads it). Returns false if the command
// timed out or nothing is connected to port 1.
inline bool intback_read_port1(uint8_t* out_d1, uint8_t* out_d2) {
    if (!wait_sf_clear()) {
        return false;
    }

    SMPC_IREG0 = 0x00u; // "get only peripheral data" (no SMPC status bytes)
    // bit7-6 P2MD=11 (0-byte mode, port 2 skipped), bit5-4 P1MD=00
    // (15-byte mode), bit3 PEN=1 (return peripheral data), bit1 OPE=1
    // (no optimization — simplest correct behavior for a polled read).
    SMPC_IREG1 = 0xCAu;
    SMPC_IREG2 = 0xF0u; // mandatory per the INTBACK command spec

    SMPC_COMREG = kIntbackCommand;
    SMPC_SF = 0x01u;

    if (!wait_sf_clear()) {
        return false;
    }

    // OREG0 = port 1 status: bits3-0 = number of connectors (0 = nothing
    // connected or an unrecognized peripheral).
    const uint8_t port1_status = SMPC_OREG0;
    if ((port1_status & 0x0Fu) == 0u) {
        return false;
    }

    // OREG1 (skipped, peripheral ID/data-size byte) then 1st/2nd data.
    *out_d1 = SMPC_OREG2;
    *out_d2 = SMPC_OREG3;
    return true;
}

// Saturn Digital Device standard format (2-byte data, peripheral type 0H —
// see docs/sega_saturn_hardware/hard/smpc/hon/p03_20.md, Table 3.10).
// Every button bit reads 0 when pressed.
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

}  // namespace saturn::hal::smpc
