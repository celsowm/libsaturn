#ifndef SATURN_INTERNAL_HPP
#define SATURN_INTERNAL_HPP

#include <stdint.h>

#include "saturn/saturn.h"

namespace saturn::internal {

constexpr uint16_t kDefaultWidth = 320;
constexpr uint16_t kDefaultHeight = 224;
constexpr uint16_t kCmdCapacity = 512;

inline int16_t fx16_to_int(sat_fx16_t value) {
    return static_cast<int16_t>(value >> 16);
}

}  // namespace saturn::internal

#endif

