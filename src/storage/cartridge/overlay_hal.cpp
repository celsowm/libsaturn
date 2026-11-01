#include "src/storage/cartridge/overlay_hal.hpp"

#include "src/hal/sh2/cache.hpp"

namespace saturn::storage::cart_overlay {

void* write_pointer(uint32_t cached_base) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(cached_base | kCacheThroughBit));
}

/* A slot is up to megabytes and the cache 4 KiB: purging it all is cheaper
 * than one associative purge per 16-byte line of the slot. */
void purge_cpu_cache() {
    saturn::hal::sh2::cache::purge_keeping_mode();
}

}  // namespace saturn::storage::cart_overlay
