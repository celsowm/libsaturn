#ifndef SATURN_HAL_SMPC_HPP
#define SATURN_HAL_SMPC_HPP

#include <stdint.h>

#include "src/hal/smpc_input_logic.hpp"

namespace saturn::hal::smpc {

bool read_digital_pad(uint8_t port, DigitalPadSample* out_sample);
bool sound_on();
bool sound_off();

}  // namespace saturn::hal::smpc

#endif
