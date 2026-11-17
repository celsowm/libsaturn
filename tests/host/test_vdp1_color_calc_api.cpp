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
static uint16_t lut_words[16];
extern "C" sat_result_t sat_vdp1_upload_lut(const uint16_t* lut,uint16_t* out) {
    for(unsigned i=0;i<16;++i)lut_words[i]=lut[i];
    *out=0x1234;return SAT_OK;
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
    /* A LUT4 texture: its table carries the selector, the draw keeps the table and claims the mode. */
    const uint8_t codes[16]={0,17,18,0,255};
    uint16_t lut=0;
    assert(sat_vdp1_upload_color_calc_lut(codes,5,SAT_VDP2_SPRITE_ADD,&lut)==SAT_OK && lut==0x1234);
    assert(lut_words[0]==0 && lut_words[1]==0x4511 && lut_words[2]==0x4512 && lut_words[3]==0 && lut_words[4]==0x45FF);
    assert(sat_vdp1_upload_color_calc_lut(codes,5,4,&lut)==SAT_OK && lut_words[1]==0x6511);
    assert(sat_vdp1_upload_color_calc_lut(codes,8,4,&lut)==SAT_ERR_INVALID_ARG);
    assert(sat_vdp1_upload_color_calc_lut(nullptr,5,4,&lut)==SAT_ERR_INVALID_ARG);
    texture.format=SAT_VDP1_TEXTURE_LUT4;texture.palette=0x0777;
    assert(sat_draw_sprite_distorted_color_calc(&distorted,SAT_VDP2_SPRITE_ADD)==SAT_OK);
    assert(draws==4 && palette==0x0777 && claim.mode==SAT_VDP2_COLOR_CALC_ADD);
    distorted.palette_override=0x0888;
    assert(sat_draw_sprite_distorted_color_calc(&distorted,SAT_VDP2_SPRITE_ADD)==SAT_OK && palette==0x0888);
    assert(sat_draw_sprite_distorted_color_calc(&distorted,9)==SAT_ERR_INVALID_ARG && draws==5);
    texture.format=7;
    assert(sat_draw_sprite_distorted_color_calc(&distorted,SAT_VDP2_SPRITE_ADD)==SAT_ERR_UNSUPPORTED && draws==5);
}
