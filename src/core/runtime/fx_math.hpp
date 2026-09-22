#ifndef SATURN_CORE_FX_MATH_HPP
#define SATURN_CORE_FX_MATH_HPP

#include "saturn/saturn.h"

namespace saturn::core {

/* fx16 (16.16 fixed point) -> integer, truncating toward negative infinity
 * (arithmetic right shift). Matches the conversion inlined in resolve_sprite_cmd.
 */
inline int32_t fx16_to_int_impl(sat_fx16_t v) {
    return static_cast<int32_t>(v >> 16);
}

}  // namespace saturn::core

#endif /* SATURN_CORE_FX_MATH_HPP */
