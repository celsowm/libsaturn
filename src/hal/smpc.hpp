#ifndef SATURN_HAL_SMPC_HPP
#define SATURN_HAL_SMPC_HPP

#include <stdint.h>

namespace saturn::hal::smpc {

uint16_t read_digital_pad();
bool sound_on();
bool sound_off();

}  // namespace saturn::hal::smpc

#endif
