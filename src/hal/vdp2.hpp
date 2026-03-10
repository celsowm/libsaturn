#ifndef SATURN_HAL_VDP2_HPP
#define SATURN_HAL_VDP2_HPP

#include <stdint.h>

namespace saturn::hal::vdp2 {

void init_ntsc_320x224();
uint16_t read_tvstat();

}  // namespace saturn::hal::vdp2

#endif

