// Rejected study candidate: 1,675.5 median sort ticks versus 1,299 for
// grouped buckets on the robot. Archived for future experiments; unused
// by production rendering. Exact stable ordering passed host checks.
#pragma once
#include "src/graphics/3d/rendering/logic.hpp"
namespace saturn::core::render3d {
/* Exact, stable ordering for scenes that fit the native command budget.
 * Four byte passes preserve source order for equal keys. Scratch stays on
 * the Master stack (4 KiB indices + 512 bytes counters); larger scenes use
 * the existing grouped painter. Original keys remain readable throughout. */
constexpr uint32_t kPaintRadixCapacity = 2048u;
inline uint32_t paint_order_radix16(const uint32_t* keys, uint32_t count,
                                   uint16_t* out) {
    if (count > kPaintRadixCapacity) return 0u;
    uint16_t scratch[kPaintRadixCapacity];
    uint16_t positions[256];
    uint32_t live = 0u;
    for (uint32_t f = 0; f < count; ++f)
        if (keys[f] != kPaintSkip) out[live++] = static_cast<uint16_t>(f);
    uint16_t* input = out;
    uint16_t* output = scratch;
    for (uint32_t shift = 0u; shift < 32u; shift += 8u) {
        for (uint32_t b = 0; b < 256u; ++b) positions[b] = 0u;
        for (uint32_t i = 0; i < live; ++i)
            ++positions[(~keys[input[i]] >> shift) & 255u];
        uint16_t running = 0u;
        for (uint32_t b = 0; b < 256u; ++b) {
            const uint16_t n = positions[b];
            positions[b] = running;
            running = static_cast<uint16_t>(running + n);
        }
        for (uint32_t i = 0; i < live; ++i) {
            const uint16_t face = input[i];
            output[positions[(~keys[face] >> shift) & 255u]++] = face;
        }
        uint16_t* swap = input; input = output; output = swap;
    }
    return live;
}

}
