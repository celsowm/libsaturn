#include <cassert>
#include "saturn/vdp1_color_calc.h"
#include "src/graphics/vdp2/color_calc_logic.hpp"

static saturn::core::vdp2_color_calc::ModeClaim claim{};
static unsigned draws;
static uint16_t palette;
static bool initialized=true;
extern "C" sat_result_t sat_vdp2_sprite_color_calc_claim_mode(sat_vdp2_color_calc_mode_t mode) {
    return initialized ? saturn::core::vdp2_color_calc::claim_mode(claim,mode) : SAT_ERR_NOT_INITIALIZED;
}
extern "C" sat_result_t sat_draw_sprite(const sat_sprite_cmd_t* cmd) {
    ++draws;palette=cmd->palette_override;return SAT_OK;
}
extern "C" sat_result_t sat_draw_sprite_scaled(const sat_scaled_sprite_cmd_t* cmd) {
    ++draws;palette=cmd->palette_override;return SAT_OK;
}
extern "C" sat_result_t sat_draw_sprite_distorted(const sat_distorted_sprite_cmd_t* cmd) {
    ++draws;palette=cmd->palette_override;return SAT_OK;
}
int main() {
    sat_vdp1_texture_t texture{};texture.valid=1;texture.format=SAT_VDP1_TEXTURE_INDEXED8;texture.palette=5;
    sat_sprite_cmd_t normal{};normal.texture=&texture;
    sat_scaled_sprite_cmd_t scaled{};scaled.texture=&texture;
    sat_distorted_sprite_cmd_t distorted{};distorted.texture=&texture;
    assert(sat_draw_sprite_distorted_color_calc(&distorted,SAT_VDP2_SPRITE_ADD)==SAT_OK);
    assert(palette==0x45 && claim.mode==SAT_VDP2_COLOR_CALC_ADD);
    assert(sat_draw_sprite_color_calc(&normal,4)==SAT_ERR_BUSY && draws==1);
    assert(distorted.palette_override==0 && texture.palette==5);
    assert(sat_draw_sprite_scaled_color_calc(&scaled,SAT_VDP2_SPRITE_ADD)==SAT_OK);
    saturn::core::vdp2_color_calc::claim_reset(claim);
    assert(sat_draw_sprite_color_calc(&normal,4)==SAT_OK && palette==0x65);
    assert(sat_draw_sprite_distorted_color_calc(&distorted,SAT_VDP2_SPRITE_ADD)==SAT_ERR_BUSY && draws==3);
    saturn::core::vdp2_color_calc::claim_reset(claim);
    initialized=false;
    assert(sat_draw_sprite_distorted_color_calc(&distorted,SAT_VDP2_SPRITE_ADD)==SAT_ERR_NOT_INITIALIZED);
    assert(draws==3);
    initialized=true;
    texture.format=SAT_VDP1_TEXTURE_LUT4;
    assert(sat_draw_sprite_distorted_color_calc(&distorted,SAT_VDP2_SPRITE_ADD)==SAT_ERR_UNSUPPORTED);
    assert(draws==3 && claim.claimed==0);
}
