#ifndef SATURN_STORAGE_CARTRIDGE_OVERLAY_HAL_HPP
#define SATURN_STORAGE_CARTRIDGE_OVERLAY_HAL_HPP

#include <stdint.h>

/* The two hardware touches of sat_cart_overlay_load, apart so that host tests
 * can point the slot at a buffer and count purges (overlay_hal.cpp on the
 * Saturn). */
namespace saturn::storage::cart_overlay {

constexpr uint32_t kCartBase = 0x02400000u;
constexpr uint32_t kCartEnd = 0x02800000u;  /* 4 MiB window */
constexpr uint32_t kCacheThroughBit = 0x20000000u;

/* Where the loader writes the slot: the cache-through mirror. */
void* write_pointer(uint32_t cached_base);
/* Drops every line of the calling CPU's cache (if the cache is on). */
void purge_cpu_cache();

}  // namespace saturn::storage::cart_overlay

#endif
