#ifndef SATURN_STORAGE_CD_EXEC_LOGIC_HPP
#define SATURN_STORAGE_CD_EXEC_LOGIC_HPP

#include <stdint.h>

#include "saturn/exec.h"

/* The pure parts of sat_exec: where Work RAM Low is, and the hand-off block a
 * program leaves for the next one. Host-tested (tests/host/test_exec.cpp). */
namespace saturn::storage::exec::logic {

/* Work RAM Low: the image is staged from its base; the last 512 bytes hold the
 * copy routine and the hand-off, so no program's .wram_l may reach them
 * (saturn.ld asserts __wram_l_end <= kReservedBase). */
constexpr uint32_t kWorkRamLowBase = 0x00200000u;
constexpr uint32_t kWorkRamLowEnd = 0x00300000u;
constexpr uint32_t kReservedBase = 0x002FFE00u;
constexpr uint32_t kTrampolineBase = kReservedBase;
constexpr uint32_t kTrampolineMax = 0x1E0u;
constexpr uint32_t kHandoffBase = 0x002FFFE0u;
constexpr uint32_t kCacheThroughBit = 0x20000000u;
constexpr uint32_t kSectorBytes = 2048u;

static_assert(kWorkRamLowBase + SAT_EXEC_MAX_BYTES <= kReservedBase, "staging overlaps the reserve");
static_assert(kTrampolineBase + kTrampolineMax <= kHandoffBase, "copy routine overlaps the hand-off");

/* Whether [address, address + bytes) touches Work RAM Low through its cached
 * or its cache-through address. */
inline bool touches_work_ram_low(uintptr_t address, uint32_t bytes) {
    if (bytes == 0u) bytes = 1u;
    const uint64_t begin = address & ~static_cast<uintptr_t>(kCacheThroughBit);
    const uint64_t end = begin + bytes;
    return begin < kWorkRamLowEnd && end > kWorkRamLowBase;
}

inline uint32_t sectors_for(uint32_t bytes) {
    return (bytes + kSectorBytes - 1u) / kSectorBytes;
}

/* 32 bytes at kHandoffBase. `check` covers bytes and data, so neither a cold
 * boot's leftover RAM nor a hand-off already read passes for a fresh one. */
struct Handoff {
    uint32_t magic;
    uint32_t bytes;
    uint32_t check;
    uint32_t reserved;
    uint8_t data[SAT_EXEC_ARG_MAX];
};
static_assert(sizeof(Handoff) == 32u, "hand-off block is 32 bytes");

constexpr uint32_t kMagic = 0x53455843u; /* 'SEXC' */

inline uint32_t checksum(const Handoff& h) {
    uint32_t hash = 0x811C9DC5u ^ h.bytes;  /* FNV-1a over the length and the data */
    for (uint32_t i = 0; i < SAT_EXEC_ARG_MAX; ++i) {
        hash = (hash ^ h.data[i]) * 0x01000193u;
    }
    return hash;
}

inline void seal(Handoff& h, const void* arg, uint32_t bytes) {
    const uint8_t* in = static_cast<const uint8_t*>(arg);
    for (uint32_t i = 0; i < SAT_EXEC_ARG_MAX; ++i) h.data[i] = i < bytes ? in[i] : 0u;
    h.reserved = 0u;
    h.bytes = bytes;
    h.check = checksum(h);
    h.magic = kMagic;
}

inline bool valid(const Handoff& h) {
    return h.magic == kMagic && h.bytes <= SAT_EXEC_ARG_MAX && h.check == checksum(h);
}

inline void forget(Handoff& h) {
    h.magic = 0u;
}

}  // namespace saturn::storage::exec::logic

#endif /* SATURN_STORAGE_CD_EXEC_LOGIC_HPP */
