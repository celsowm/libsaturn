#include "src/hal/smpc/smpc.hpp"

namespace saturn::hal::smpc {

namespace {

#define SMPC_IREG0 (*reinterpret_cast<volatile uint8_t*>(0x20100001))
#define SMPC_IREG1 (*reinterpret_cast<volatile uint8_t*>(0x20100003))
#define SMPC_IREG2 (*reinterpret_cast<volatile uint8_t*>(0x20100005))
#define SMPC_IREG(n) (*reinterpret_cast<volatile uint8_t*>(0x20100001u + 2u * (n)))
#define SMPC_OREG(n) (*reinterpret_cast<volatile uint8_t*>(0x20100021u + 2u * (n)))
#define SMPC_SR (*reinterpret_cast<volatile uint8_t*>(0x20100061))
#define SCU_IST (*reinterpret_cast<volatile uint32_t*>(0x25FE00A4))
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
constexpr uint8_t kSlaveOnCommand = 0x02u;
constexpr uint8_t kSlaveOffCommand = 0x03u;
constexpr uint8_t kResetEnableCommand = 0x19u;
constexpr uint8_t kResetDisableCommand = 0x1Au;

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

/* IST bits latch whether or not the source is masked; writing 0 clears. */
inline void clear_smpc_ist() {
    SCU_IST = ~kIstSmpc;
}

/* A chunk is ready when the SMPC has both dropped SF and raised its
 * interrupt status (a masked source still latches). Without the second
 * check a CONTINUE that the SMPC ignored, because the INTBACK had already
 * timed out at VBlank-IN, would hand back the previous chunk again. */
inline bool wait_chunk_ready() {
    uint32_t spins = 0;
    while ((SMPC_SF & 0x01u) != 0u || (SCU_IST & kIstSmpc) == 0u) {
        if (++spins >= kSfTimeoutIters) return false;
    }
    return true;
}

inline void break_intback() {
    SMPC_IREG(0) = kIntbackBreak;
    (void)wait_sf_clear();
}

inline void init_control_mode_once() {
    static bool initialized = false;
    if (!initialized) {
        ensure_smpc_control_mode();
        initialized = true;
    }
}

}  // namespace

bool read_peripherals(PeripheralSnapshot* out_snapshot, uint8_t port_mask, ParseStatus* parse) {
    if (parse != nullptr) *parse = ParseStatus::Ok;
    port_mask = static_cast<uint8_t>(port_mask & kIntbackPortsBoth);
    if (out_snapshot == nullptr || port_mask == 0u) return false;
    init_control_mode_once();
    if (!wait_sf_clear()) return false;

    ChunkStream stream;      /* only bytes[0..length) are ever read */
    stream.length = 0u;
    stream.chunks = 0u;
    stream.overflow = false;
    clear_smpc_ist();
    SMPC_IREG(0) = 0x00u;
    SMPC_IREG(1) = intback_ireg1(port_mask);
    SMPC_IREG(2) = 0xF0u;
    SMPC_SF = 0x01u;
    SMPC_COMREG = kCmdIntback;
    if (!wait_sf_clear()) return false;

    for (;;) {
        uint8_t oreg[kChunkBytes];
        for (uint8_t i = 0u; i < kChunkBytes; ++i) oreg[i] = SMPC_OREG(i);
        const uint8_t sr = SMPC_SR;
        if (!add_chunk(&stream, oreg)) {
            break_intback();
            return false;
        }
        if (!more_chunks(sr)) break;
        clear_smpc_ist();
        SMPC_IREG(0) = kIntbackContinue;
        if (!wait_chunk_ready()) {
            break_intback();
            return false;
        }
    }
    const bool enabled[kPortCount] = {(port_mask & 0x1u) != 0u, (port_mask & 0x2u) != 0u};
    const ParseStatus st = parse_snapshot(stream.bytes, stream.length, enabled, out_snapshot);
    if (parse != nullptr) *parse = st;
    return st == ParseStatus::Ok;
}

bool read_status(SmpcStatus* out_status) {
    if (out_status == nullptr) return false;
    init_control_mode_once();
    if (!wait_sf_clear()) return false;
    SMPC_IREG(0) = 0x01u;   /* status block, no peripheral data (PEN = 0) */
    SMPC_IREG(1) = 0x00u;
    SMPC_IREG(2) = 0xF0u;
    SMPC_SF = 0x01u;
    SMPC_COMREG = kCmdIntback;
    if (!wait_sf_clear()) return false;
    uint8_t oreg[16];
    for (uint8_t i = 0u; i < 16u; ++i) oreg[i] = SMPC_OREG(i);
    return parse_status(oreg, out_status);
}

bool set_time(const RtcTime& time) {
    if (!valid_time(time)) return false;
    init_control_mode_once();
    if (!wait_sf_clear()) return false;
    uint8_t ireg[7];
    encode_settime(time, ireg);
    for (uint8_t i = 0u; i < 7u; ++i) SMPC_IREG(i) = ireg[i];
    SMPC_SF = 0x01u;
    SMPC_COMREG = kCmdSetTime;
    if (!wait_sf_clear()) return false;
    return SMPC_OREG(31) == kCmdSetTime;
}

bool set_smem(const uint8_t smem[4]) {
    if (smem == nullptr) return false;
    init_control_mode_once();
    if (!wait_sf_clear()) return false;
    for (uint8_t i = 0u; i < 4u; ++i) SMPC_IREG(i) = smem[i];
    SMPC_SF = 0x01u;
    SMPC_COMREG = kCmdSetSmem;
    if (!wait_sf_clear()) return false;
    return SMPC_OREG(31) == kCmdSetSmem;
}

bool sound_on() {
    ensure_smpc_control_mode();
    return issue_simple_command(kSoundOnCommand);
}

bool sound_off() {
    ensure_smpc_control_mode();
    return issue_simple_command(kSoundOffCommand);
}

bool slave_on() {
    ensure_smpc_control_mode();
    return issue_simple_command(kSlaveOnCommand);
}

bool slave_off() {
    ensure_smpc_control_mode();
    return issue_simple_command(kSlaveOffCommand);
}

bool reset_enable() {
    ensure_smpc_control_mode();
    return issue_simple_command(kResetEnableCommand);
}

bool reset_disable() {
    ensure_smpc_control_mode();
    return issue_simple_command(kResetDisableCommand);
}

}  // namespace saturn::hal::smpc
