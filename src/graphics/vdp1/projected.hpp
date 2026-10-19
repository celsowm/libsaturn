#ifndef SATURN_GRAPHICS_VDP1_PROJECTED_HPP
#define SATURN_GRAPHICS_VDP1_PROJECTED_HPP

#include "saturn/render3d.h"

namespace saturn::internal {
/* Scene-only opaque path. Coordinates are already in native command space;
 * retain descriptor, effect and command-budget checks at emission time. */
sat_result_t draw_projected_sprite(const sat_quad2_t& quad,
    const sat_vdp1_texture_t& texture, uint16_t flags);
sat_result_t draw_projected_program(const sat_quad2_t& quad,
    const sat_vdp1_projected_program_t& program);
}
#endif
