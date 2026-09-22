#ifndef SATURN_CORE_SPATIAL3_HASH_HPP
#define SATURN_CORE_SPATIAL3_HASH_HPP

#include <stdint.h>

#include "saturn/collide3d.h"

namespace saturn::core::spatial3_hash {

inline bool power_of_two(uint16_t value) {
    return value != 0u && (value & static_cast<uint16_t>(value - 1u)) == 0u;
}

inline int32_t cell(int64_t raw, uint8_t shift) {
    const int64_t size = static_cast<int64_t>(1) << (16u + shift);
    if (raw >= 0) return static_cast<int32_t>(raw / size);
    return static_cast<int32_t>(-(((-raw) + size - 1) / size));
}

inline uint16_t bucket(uint16_t bucket_count, int32_t x, int32_t y, int32_t z) {
    uint32_t h = static_cast<uint32_t>(x) * 73856093u;
    h ^= static_cast<uint32_t>(y) * 19349663u;
    h ^= static_cast<uint32_t>(z) * 83492791u;
    h ^= h >> 16u;
    return static_cast<uint16_t>(h & static_cast<uint32_t>(bucket_count - 1u));
}

inline void aabb_cells(
    const sat_aabb3_t& box,
    uint8_t shift,
    int32_t& x0, int32_t& y0, int32_t& z0,
    int32_t& x1, int32_t& y1, int32_t& z1
) {
    x0 = cell(static_cast<int64_t>(box.center.x) - box.half.x, shift);
    y0 = cell(static_cast<int64_t>(box.center.y) - box.half.y, shift);
    z0 = cell(static_cast<int64_t>(box.center.z) - box.half.z, shift);
    x1 = cell(static_cast<int64_t>(box.center.x) + box.half.x, shift);
    y1 = cell(static_cast<int64_t>(box.center.y) + box.half.y, shift);
    z1 = cell(static_cast<int64_t>(box.center.z) + box.half.z, shift);
}

inline uint64_t cell_count(
    int32_t x0, int32_t y0, int32_t z0,
    int32_t x1, int32_t y1, int32_t z1
) {
    const uint64_t nx = static_cast<uint64_t>(static_cast<int64_t>(x1) - x0 + 1);
    const uint64_t ny = static_cast<uint64_t>(static_cast<int64_t>(y1) - y0 + 1);
    const uint64_t nz = static_cast<uint64_t>(static_cast<int64_t>(z1) - z0 + 1);
    return nx * ny * nz;
}

}  // namespace saturn::core::spatial3_hash

#endif /* SATURN_CORE_SPATIAL3_HASH_HPP */
