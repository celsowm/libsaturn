#ifndef SATURN_CORE_INPUT_LOGIC_HPP
#define SATURN_CORE_INPUT_LOGIC_HPP

#include "saturn/saturn.h"

namespace saturn::core {

inline sat_pad_state_t compute_pad_state(uint16_t prev_held, uint16_t cur_held) {
    sat_pad_state_t state = {};
    state.held = cur_held;
    state.pressed = static_cast<uint16_t>((~prev_held) & cur_held);
    state.released = static_cast<uint16_t>(prev_held & (~cur_held));
    return state;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_INPUT_LOGIC_HPP */
