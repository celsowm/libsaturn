#ifndef SATURN_CORE_STARTUP_MEMORY_LAYOUT_HPP
#define SATURN_CORE_STARTUP_MEMORY_LAYOUT_HPP

#include <stdint.h>

namespace saturn::core::startup {

/* These addresses are part of the Saturn boot contract.  The BIOS owns the
 * vector tables and the initial slave stack; the application owns the rest of
 * the first 16 KiB only through the dual-SH2 control block below. */
constexpr uint32_t kWorkRamHighBase = 0x06000000u;
constexpr uint32_t kWorkRamHighEnd = 0x06100000u;
constexpr uint32_t kMasterVectorBase = 0x06000000u;
constexpr uint32_t kSlaveVectorBase = 0x06000400u;
constexpr uint32_t kSlaveStackTop = 0x06001000u;
constexpr uint32_t kDualSh2ControlBase = 0x06002000u;
constexpr uint32_t kDualSh2ControlEnd = 0x06004000u;
constexpr uint32_t kApplicationLoadAddress = 0x06004000u;
constexpr uint32_t kSlaveEntryVector = 0x94u;
constexpr uint32_t kSlaveEntryVectorAddress =
    kSlaveVectorBase + (kSlaveEntryVector * sizeof(uint32_t));

constexpr uint32_t kCacheThroughBit = 0x20000000u;
constexpr uint32_t kCachePurgeBit = 0x40000000u;

static_assert(kMasterVectorBase + 0x400u <= kSlaveVectorBase);
static_assert(kSlaveVectorBase + 0x400u <= kSlaveStackTop);
static_assert(kSlaveStackTop < kDualSh2ControlBase);
static_assert(kDualSh2ControlBase < kDualSh2ControlEnd);
static_assert(kDualSh2ControlEnd == kApplicationLoadAddress);

}  // namespace saturn::core::startup

#endif
