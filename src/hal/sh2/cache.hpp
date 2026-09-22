#ifndef SATURN_HAL_SH2_CACHE_HPP
#define SATURN_HAL_SH2_CACHE_HPP

#include <stdint.h>

namespace saturn::hal::sh2::cache {

enum class Mode : uint8_t {
    Cache4KiB = 0x01,
    Cache2KiBAndRam2KiB = 0x09,
};

constexpr uint32_t kLineBytes = 16u;

uint8_t control_register();
void purge_all(Mode mode);
bool invalidate_line(uint32_t cached_address);
bool is_supported_work_ram(uint32_t address, uint32_t size = 1u);
uint32_t cache_through(uint32_t address);
uint32_t purge_alias(uint32_t address);

}  // namespace saturn::hal::sh2::cache

#endif
