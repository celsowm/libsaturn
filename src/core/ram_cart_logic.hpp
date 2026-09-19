#ifndef SATURN_CORE_RAM_CART_LOGIC_HPP
#define SATURN_CORE_RAM_CART_LOGIC_HPP

#include <stddef.h>
#include <stdint.h>

namespace saturn::core::ram_cart_logic {

struct Geometry {
    uint8_t banks;
    uint32_t bytes_per_bank;
};
constexpr Geometry geometry(uint8_t id) {
    return id == 0x5Au ? Geometry{2u, 512u * 1024u}
         : id == 0x5Cu ? Geometry{2u, 2u * 1024u * 1024u}
                       : Geometry{0u, 0u};
}
constexpr bool valid_alignment(size_t alignment) {
    return alignment != 0u && (alignment & (alignment - 1u)) == 0u;
}
inline size_t available_after_alignment(const uint8_t* base, size_t used,
                                        size_t capacity, size_t alignment) {
    if (base == nullptr || used > capacity || !valid_alignment(alignment)) return 0u;
    const uintptr_t start = reinterpret_cast<uintptr_t>(base);
    if (start > static_cast<uintptr_t>(-1) - used) return 0u;
    const uintptr_t current = start + used;
    const uintptr_t mask = static_cast<uintptr_t>(alignment - 1u);
    if (current > static_cast<uintptr_t>(-1) - mask) return 0u;
    const uintptr_t aligned = (current + mask) & ~mask;
    const size_t offset = static_cast<size_t>(aligned - start);
    return offset <= capacity ? capacity - offset : 0u;
}
} // namespace saturn::core::ram_cart_logic
#endif
