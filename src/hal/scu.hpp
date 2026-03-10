#ifndef SATURN_HAL_SCU_HPP
#define SATURN_HAL_SCU_HPP

#include <stdint.h>

namespace saturn::hal::scu {

void init_interrupts();
void wait_vblank();
uint32_t frame_counter();

}  // namespace saturn::hal::scu

#endif

