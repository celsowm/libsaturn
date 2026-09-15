#ifndef SATURN_HAL_SCU_HPP
#define SATURN_HAL_SCU_HPP

#include <stdint.h>

namespace saturn::hal::scu {

void init_interrupts();
/* Calibrates the display-frame clock; needs the display already enabled. */
void init_frame_clock();
void wait_vblank();
/* sat_wait_vblank calls, i.e. program frames. */
uint32_t frame_counter();
/* Display frames elapsed since init_frame_clock, including missed ones. */
uint32_t display_frames();

}  // namespace saturn::hal::scu

#endif

