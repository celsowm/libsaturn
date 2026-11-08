#ifndef SATURN_CORE_TIME_LOGIC_HPP
#define SATURN_CORE_TIME_LOGIC_HPP

#include <stdint.h>

#include "src/core/math3d/logic.hpp"

namespace saturn::core::time_logic {

inline uint32_t elapsed_ms(uint32_t now, uint32_t then) {
    return now - then;
}

inline uint32_t ticks_to_ms(uint64_t elapsed_ticks, uint16_t ticks_per_frame, uint32_t frames_per_second) {
    if (ticks_per_frame == 0u || frames_per_second == 0u) {
        return 0u;
    }
    const uint64_t ticks_per_second = static_cast<uint64_t>(ticks_per_frame) * frames_per_second;
    const uint64_t scaled = elapsed_ticks * 1000u;
    /* Below 2^31 ms (24 days) the quotient fits the on-chip divider's signed
     * 32 bits and both operands are positive, so it is the same floor as the
     * 64-bit division (about 40 cycles instead of __udivdi3); beyond, libgcc. */
    if (ticks_per_second <= static_cast<uint64_t>(INT32_MAX) &&
        scaled <= static_cast<uint64_t>(INT32_MAX) * ticks_per_second) {
        return static_cast<uint32_t>(saturn::core::math3d::div_s64_s32(
            static_cast<int64_t>(scaled), static_cast<int32_t>(ticks_per_second)));
    }
    return static_cast<uint32_t>(scaled / ticks_per_second);
}

}  // namespace saturn::core::time_logic

#endif /* SATURN_CORE_TIME_LOGIC_HPP */
