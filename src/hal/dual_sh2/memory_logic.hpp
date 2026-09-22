#ifndef SATURN_HAL_DUAL_SH2_MEMORY_LOGIC_HPP
#define SATURN_HAL_DUAL_SH2_MEMORY_LOGIC_HPP

#include <stdint.h>

#include "src/core/startup/memory_layout.hpp"

namespace saturn::hal::dual_sh2::memory_logic {

constexpr uint32_t kWorkRamLowBase = 0x00200000u;
constexpr uint32_t kWorkRamLowEnd = 0x00300000u;

inline bool in_range(uint32_t address, uint32_t size,
                     uint32_t begin, uint32_t end) {
    if (size == 0u || address < begin || address >= end) return false;
    return size <= end - address;
}

inline bool is_work_ram(uint32_t address, uint32_t size = 1u) {
    return in_range(address, size, saturn::core::startup::kWorkRamHighBase,
                    saturn::core::startup::kWorkRamHighEnd) ||
           in_range(address, size, kWorkRamLowBase, kWorkRamLowEnd);
}

inline uint32_t uncached(uint32_t physical_address) {
    return is_work_ram(physical_address)
        ? physical_address | saturn::core::startup::kCacheThroughBit : 0u;
}

inline uint32_t cached(uint32_t address) {
    const uint32_t physical = address & ~saturn::core::startup::kCacheThroughBit;
    if (!is_work_ram(physical)) return 0u;
    return physical;
}

}  // namespace saturn::hal::dual_sh2::memory_logic

#endif
