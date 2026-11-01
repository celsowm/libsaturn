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
/* Purges the calling CPU's cache in its present mode, from the cache-through
 * area as the manual wants for a CCR write. Does nothing while the cache is off. */
void purge_keeping_mode();
/* Puts the calling CPU's cache in the full 4 KiB mode, whatever the BIOS or a
 * loader left, and records what it found. `cpu` is 0 for Master, 1 for Slave. */
void enable_at_start(uint8_t cpu);
uint8_t control_at_start(uint8_t cpu);
bool invalidate_line(uint32_t cached_address);
/* Invalidates every line overlapping [address, address + size). Returns
 * false, touching nothing, unless the whole span is cacheable Work RAM. */
bool invalidate_range(uint32_t cached_address, uint32_t size);
bool is_supported_work_ram(uint32_t address, uint32_t size = 1u);
uint32_t cache_through(uint32_t address);
uint32_t purge_alias(uint32_t address);

}  // namespace saturn::hal::sh2::cache

#endif
