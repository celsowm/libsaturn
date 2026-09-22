#include "src/hal/sh2/interrupt.hpp"

#include "src/hal/sh2/cache.hpp"
#include "src/hal/sh2/cpu.hpp"

namespace saturn::hal::sh2::interrupt {

namespace {

uint32_t table_address() {
    return read_vector_base();
}

volatile uint32_t* entry(uint8_t vector) {
    const uint32_t table = table_address();
    const uint32_t uncached = cache::cache_through(table);
    if (uncached == 0u) return nullptr;
    return reinterpret_cast<volatile uint32_t*>(uncached +
                                                 (static_cast<uint32_t>(vector) * 4u));
}

}  // namespace

uint32_t vector_address(uint8_t vector) {
    const uint32_t table = table_address();
    return table == 0u ? 0u : table + (static_cast<uint32_t>(vector) * 4u);
}

Handler read(uint8_t vector) {
    volatile uint32_t* const slot = entry(vector);
    return slot == nullptr ? nullptr : reinterpret_cast<Handler>(*slot);
}

Handler install(uint8_t vector, Handler handler) {
    volatile uint32_t* const slot = entry(vector);
    if (slot == nullptr) return nullptr;
    const Handler previous = reinterpret_cast<Handler>(*slot);
    *slot = reinterpret_cast<uint32_t>(handler);
    compiler_barrier();
    return previous;
}

void restore(uint8_t vector, Handler handler) {
    volatile uint32_t* const slot = entry(vector);
    if (slot == nullptr) return;
    *slot = reinterpret_cast<uint32_t>(handler);
    compiler_barrier();
}

}  // namespace saturn::hal::sh2::interrupt
