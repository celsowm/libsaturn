#include "src/storage/cartridge/overlay_hal.hpp"

#include "src/hal/sh2/cache.hpp"

namespace saturn::storage::cart_overlay {

void* write_pointer(uint32_t cached_base) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(cached_base | kCacheThroughBit));
}

/* A slot is up to megabytes and the cache 4 KiB: purging it all is cheaper
 * than one associative purge per 16-byte line of the slot. The purge writes
 * CCR, which the manual wants done from the cache-through area: call it
 * there (as the Slave purge in core/parallel does). */
void purge_cpu_cache() {
    namespace cache = saturn::hal::sh2::cache;
    const uint8_t ccr = cache::control_register();
    if ((ccr & 0x01u) == 0u) return;
    using Fn = void (*)(cache::Mode);
    const Fn purge = reinterpret_cast<Fn>(
        reinterpret_cast<uintptr_t>(&cache::purge_all) | kCacheThroughBit);
    purge(static_cast<cache::Mode>(ccr & 0x09u));
}

}  // namespace saturn::storage::cart_overlay
