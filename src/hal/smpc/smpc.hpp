#ifndef SATURN_HAL_SMPC_HPP
#define SATURN_HAL_SMPC_HPP

#include <stdint.h>

#include "src/hal/smpc/input_logic.hpp"

namespace saturn::hal::smpc {

bool read_digital_pad(uint8_t port, DigitalPadSample* out_sample);
bool sound_on();
bool sound_off();
/* SSHON/SSHOFF are master-owned operations.  They do not make any claim
 * about application-level slave initialization or communication state. */
bool slave_on();
bool slave_off();
bool reset_enable();
bool reset_disable();

}  // namespace saturn::hal::smpc

#endif
