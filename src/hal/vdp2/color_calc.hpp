#ifndef SATURN_HAL_VDP2_COLOR_CALC_HPP
#define SATURN_HAL_VDP2_COLOR_CALC_HPP

#include <stdint.h>

#include "saturn/vdp2_color_calc.h"

namespace saturn::hal::vdp2_color_calc {

void configure(const sat_vdp2_sprite_color_calc_config_t& config);
void set_ratio(uint8_t slot, uint8_t ratio);
void disable();
void commit();
sat_result_t select_alpha_slot(uint8_t alpha, uint8_t* out_slot, uint8_t* out_alpha);
void set_strict_alpha(bool strict);
/* Frame-scoped claim on CCMD (ratio vs add); see sat_vdp2_sprite_color_calc_claim_mode. */
sat_result_t claim_mode(uint8_t mode);
/* Called by sat_begin_frame: a new frame may claim either mode again. */
void begin_frame();

}  // namespace saturn::hal::vdp2_color_calc

#endif /* SATURN_HAL_VDP2_COLOR_CALC_HPP */
