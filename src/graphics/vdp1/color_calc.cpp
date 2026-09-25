#include "saturn/vdp1_color_calc.h"

#include "saturn/vdp2_color_calc.h"
#include "src/graphics/vdp1/color_calc_logic.hpp"

namespace {

template <typename T>
sat_result_t packed_palette_for(const T* cmd, uint8_t slot, uint16_t* out) {
    if (cmd == nullptr || out == nullptr || cmd->texture == nullptr || cmd->texture->valid == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    /* The VDP2 slot rides in color-bank bits a lookup table does not have. */
    if (cmd->texture->format != SAT_VDP1_TEXTURE_INDEXED8) return SAT_ERR_UNSUPPORTED;
    const uint16_t bank = (cmd->palette_override != 0u)
        ? cmd->palette_override
        : cmd->texture->palette;
    SAT_TRY(saturn::core::vdp1_color_calc::encode_palette_selector(bank, slot, out));
    /* A slot is a ratio: refuse a frame that already went additive. With
     * colour calculation disabled the selector has no effect to protect. */
    const sat_result_t claim = sat_vdp2_sprite_color_calc_claim_mode(SAT_VDP2_COLOR_CALC_RATIO);
    return claim == SAT_ERR_NOT_INITIALIZED ? SAT_OK : claim;
}

}  // namespace

extern "C" sat_result_t sat_draw_sprite_color_calc(
    const sat_sprite_cmd_t* cmd,
    uint8_t color_calc_slot
) {
    uint16_t selector = 0u;
    sat_result_t st = packed_palette_for(cmd, color_calc_slot, &selector);
    if (st != SAT_OK) {
        return st;
    }
    sat_sprite_cmd_t copy = *cmd;
    copy.palette_override = selector;
    return sat_draw_sprite(&copy);
}

extern "C" sat_result_t sat_draw_sprite_scaled_color_calc(
    const sat_scaled_sprite_cmd_t* cmd,
    uint8_t color_calc_slot
) {
    uint16_t selector = 0u;
    sat_result_t st = packed_palette_for(cmd, color_calc_slot, &selector);
    if (st != SAT_OK) {
        return st;
    }
    sat_scaled_sprite_cmd_t copy = *cmd;
    copy.palette_override = selector;
    return sat_draw_sprite_scaled(&copy);
}

extern "C" sat_result_t sat_draw_sprite_distorted_color_calc(
    const sat_distorted_sprite_cmd_t* cmd,
    uint8_t color_calc_slot
) {
    uint16_t selector = 0u;
    sat_result_t st = packed_palette_for(cmd, color_calc_slot, &selector);
    if (st != SAT_OK) {
        return st;
    }
    sat_distorted_sprite_cmd_t copy = *cmd;
    copy.palette_override = selector;
    return sat_draw_sprite_distorted(&copy);
}
