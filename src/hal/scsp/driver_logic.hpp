#ifndef SATURN_HAL_SCSP_DRIVER_LOGIC_HPP
#define SATURN_HAL_SCSP_DRIVER_LOGIC_HPP

#include <stdint.h>

/* Pure policy of the resident 68000 sound driver: where the mailbox lives, how
 * the event ring is used and how driver ticks relate to time. The layout must
 * match sound_driver.m68k (tests/tools/test_sound_driver.py checks the offsets
 * against the assembly source). */
namespace saturn::hal::scsp::driver_logic {

/* Sound RAM offsets, as the 68000 (and, plus 0x25A00000, the SH-2) sees them. */
constexpr uint32_t kMailbox = 0x0800u;
constexpr uint32_t kMagicOffset = kMailbox + 0x00u;      /* long 'SDRV' */
constexpr uint32_t kVersionOffset = kMailbox + 0x04u;
constexpr uint32_t kFlagsOffset = kMailbox + 0x06u;      /* bit 0: running */
constexpr uint32_t kHeartbeatOffset = kMailbox + 0x08u;
constexpr uint32_t kTickOffset = kMailbox + 0x0Cu;       /* long: high word, then low word */
constexpr uint32_t kHeadOffset = kMailbox + 0x10u;
constexpr uint32_t kTailOffset = kMailbox + 0x12u;
constexpr uint32_t kExecutedOffset = kMailbox + 0x14u;
constexpr uint32_t kMaxLateOffset = kMailbox + 0x16u;
constexpr uint32_t kControlOffset = kMailbox + 0x18u;
constexpr uint32_t kLastLateOffset = kMailbox + 0x1Au;
constexpr uint32_t kRingOffset = kMailbox + 0x40u;
constexpr uint32_t kLogOffset = kMailbox + 0x300u;
constexpr uint32_t kMailboxBytes = 0x400u;               /* cleared before the driver starts */

constexpr uint32_t kRingEntries = 64u;
constexpr uint32_t kRingMask = kRingEntries - 1u;
constexpr uint32_t kEntryBytes = 8u;
constexpr uint32_t kLogEntries = 64u;
constexpr uint32_t kMagic = 0x53445256u;                 /* 'SDRV' */
constexpr uint16_t kVersion = 1u;

constexpr uint16_t kMarkerBit = 0x8000u;                 /* an event that only takes a timestamp */

/* One tick is one interrupt of SCSP timer A running free: 256 samples. */
constexpr uint32_t kSampleRate = 44100u;
constexpr uint32_t kTickSamples = 256u;

constexpr uint32_t ticks_from_ms(uint32_t ms) {
    return static_cast<uint32_t>((static_cast<uint64_t>(ms) * kSampleRate + 128000u) / 256000u);
}

constexpr uint32_t ms_from_ticks(uint32_t ticks) {
    return static_cast<uint32_t>((static_cast<uint64_t>(ticks) * 256000u + kSampleRate / 2u) / kSampleRate);
}

/* Ticks per second times one hundred, for display: 17227. */
constexpr uint32_t ticks_per_second_x100() {
    return kSampleRate * 100u / kTickSamples;
}

constexpr uint32_t ring_count(uint16_t head, uint16_t tail) {
    return (static_cast<uint32_t>(head) - tail) & kRingMask;
}

constexpr bool ring_full(uint16_t head, uint16_t tail) {
    return ring_count(head, tail) == kRingMask;
}

constexpr uint32_t ring_entry_offset(uint32_t index) {
    return kRingOffset + (index & kRingMask) * kEntryBytes;
}

constexpr uint32_t log_entry_offset(uint32_t index) {
    return kLogOffset + (index & (kLogEntries - 1u)) * 4u;
}

/* SCSP registers the 68000 can write: even offsets inside the 4 KiB block. */
constexpr bool register_ok(uint32_t offset) {
    return offset < 0x1000u && (offset & 1u) == 0u;
}

constexpr uint32_t combine_tick(uint16_t high, uint16_t low) {
    return (static_cast<uint32_t>(high) << 16u) | low;
}

}  // namespace saturn::hal::scsp::driver_logic

#endif  // SATURN_HAL_SCSP_DRIVER_LOGIC_HPP
