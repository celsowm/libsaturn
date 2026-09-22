#include "src/hal/sh2/cache.hpp"

#include "src/core/startup/memory_layout.hpp"
#include "src/hal/sh2/cpu.hpp"

namespace saturn::hal::sh2::cache {

namespace {

constexpr uint32_t kCcrAddress = 0xFFFFFE92u;
constexpr uint32_t kCcrCp = 0x10u;
constexpr uint32_t kWorkRamLowBase = 0x00200000u;
constexpr uint32_t kWorkRamLowEnd = 0x00300000u;

volatile uint8_t& ccr() {
    return *reinterpret_cast<volatile uint8_t*>(kCcrAddress);
}

bool range_in(uint32_t address, uint32_t size, uint32_t begin, uint32_t end) {
    if (size == 0u || address < begin || address >= end) return false;
    return size <= (end - address);
}

}  // namespace

uint8_t control_register() {
    return ccr();
}

void purge_all(Mode mode) {
    /* CP is a command bit: set it first, then select the post-purge mode. */
    ccr() = kCcrCp;
    ccr() = static_cast<uint8_t>(mode);
    compiler_barrier();
}

bool is_supported_work_ram(uint32_t address, uint32_t size) {
    return range_in(address, size, saturn::core::startup::kWorkRamHighBase,
                    saturn::core::startup::kWorkRamHighEnd) ||
           range_in(address, size, kWorkRamLowBase, kWorkRamLowEnd);
}

uint32_t cache_through(uint32_t address) {
    if (!is_supported_work_ram(address)) return 0u;
    return address | saturn::core::startup::kCacheThroughBit;
}

uint32_t purge_alias(uint32_t address) {
    if (!is_supported_work_ram(address)) return 0u;
    return address | saturn::core::startup::kCachePurgeBit;
}

bool invalidate_line(uint32_t cached_address) {
    if (!is_supported_work_ram(cached_address)) return false;
    const uint32_t aligned = cached_address & ~(kLineBytes - 1u);
    volatile uint16_t* const purge =
        reinterpret_cast<volatile uint16_t*>(purge_alias(aligned));
    *purge = 0u;
    compiler_barrier();
    return true;
}

}  // namespace saturn::hal::sh2::cache
