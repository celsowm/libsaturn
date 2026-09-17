#include "src/hal/smpc.hpp"

namespace saturn::hal::smpc {

namespace {

#define SMPC_IREG0 (*reinterpret_cast<volatile uint8_t*>(0x20100001))
#define SMPC_IREG1 (*reinterpret_cast<volatile uint8_t*>(0x20100003))
#define SMPC_IREG2 (*reinterpret_cast<volatile uint8_t*>(0x20100005))
#define SMPC_OREG0 (*reinterpret_cast<volatile uint8_t*>(0x20100021))
#define SMPC_OREG1 (*reinterpret_cast<volatile uint8_t*>(0x20100023))
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
        if (++spins >= kSfTimeoutIters) return false;
    }
    return true;
}

inline bool issue_simple_command(uint8_t command) {
    if (!wait_sf_clear()) return false;
    SMPC_SF = 0x01u;
    SMPC_COMREG = command;
    if (!wait_sf_clear()) return false;
    return SMPC_OREG31 == command;
}

inline bool intback_read_selected_port(
    uint8_t port,
    uint8_t* out_status,
    uint8_t* out_id,
    uint8_t* out_d1,
    uint8_t* out_d2
) {
    if (port >= SAT_PAD_PORT_COUNT || out_status == nullptr || out_id == nullptr ||
        out_d1 == nullptr || out_d2 == nullptr) {
        return false;
    }
    if (!wait_sf_clear()) return false;

    SMPC_IREG0 = 0x00u;
    SMPC_IREG1 = intback_mode_for_port(port);
    SMPC_IREG2 = 0xF0u;
    SMPC_SF = 0x01u;
    SMPC_COMREG = kIntbackCommand;

    if (!wait_sf_clear()) return false;
    *out_status = SMPC_OREG0;
    *out_id = SMPC_OREG1;
    *out_d1 = SMPC_OREG2;
    *out_d2 = SMPC_OREG3;
    return true;
}

}  // namespace

bool read_digital_pad(uint8_t port, DigitalPadSample* out_sample) {
    if (port >= SAT_PAD_PORT_COUNT || out_sample == nullptr) return false;

    static bool initialized = false;
    if (!initialized) {
        ensure_smpc_control_mode();
        initialized = true;
    }

    uint8_t status = 0u;
    uint8_t id = 0u;
    uint8_t d1 = 0xFFu;
    uint8_t d2 = 0xFFu;
    if (!intback_read_selected_port(port, &status, &id, &d1, &d2)) return false;
    *out_sample = decode_direct_digital_pad(status, id, d1, d2);
    return true;
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
