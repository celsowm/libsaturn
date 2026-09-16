#ifndef SATURN_HAL_VDP2_COLOR_CALC_HPP
#define SATURN_HAL_VDP2_COLOR_CALC_HPP

#include <stdint.h>

#include "saturn/vdp2_color_calc.h"

namespace saturn::hal::vdp2_color_calc {

void configure(const sat_vdp2_sprite_color_calc_config_t& config);
void set_ratio(uint8_t slot, uint8_t ratio);
void disable();
void commit();

}  // namespace saturn::hal::vdp2_color_calc

#endif /* SATURN_HAL_VDP2_COLOR_CALC_HPP */
