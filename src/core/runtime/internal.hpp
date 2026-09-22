#ifndef SATURN_INTERNAL_HPP
#define SATURN_INTERNAL_HPP

#include <stdint.h>

#include "saturn/core.h"

namespace saturn::internal {

constexpr uint16_t kDefaultWidth = 320;
constexpr uint16_t kDefaultHeight = 224;
/* Must fit src/hal/vdp1/vdp1.cpp's kCommandAreaBytes at 32 bytes per command. */
constexpr uint16_t kCmdCapacity = 2048;

inline int16_t fx16_to_int(sat_fx16_t value) {
    return static_cast<int16_t>(value >> 16);
}

/* Screen coordinates put (0,0) at the top-left corner; the VDP1 draws in
 * "local" coordinates whose origin sat_begin_frame places at the screen
 * centre. Converting is a single subtraction, but doing it by hand at every
 * call site is exactly how HUD elements end up drawn off-screen. */
inline int16_t screen_to_native(int coord, uint16_t extent) {
    return static_cast<int16_t>(coord - static_cast<int>(extent / 2u));
}

}  // namespace saturn::internal

#endif

