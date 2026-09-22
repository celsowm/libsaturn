#ifndef SATURN_CORE_TIME_LOGIC_HPP
#define SATURN_CORE_TIME_LOGIC_HPP

#include <stdint.h>

namespace saturn::core::time_logic {

inline uint32_t elapsed_ms(uint32_t now, uint32_t then) {
    return now - then;
}

inline uint32_t ticks_to_ms(uint64_t elapsed_ticks, uint16_t ticks_per_frame, uint32_t frames_per_second) {
    if (ticks_per_frame == 0u || frames_per_second == 0u) {
        return 0u;
    }
    const uint64_t ticks_per_second = static_cast<uint64_t>(ticks_per_frame) * frames_per_second;
    return static_cast<uint32_t>((elapsed_ticks * 1000u) / ticks_per_second);
}

}  // namespace saturn::core::time_logic

#endif /* SATURN_CORE_TIME_LOGIC_HPP */
