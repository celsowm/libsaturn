#include "saturn/vdp1_color_calc.h"

#include "saturn/vdp2_color_calc.h"
#include "src/graphics/vdp1/color_calc_logic.hpp"

namespace {

template <typename T>
sat_result_t packed_palette_for(const T* cmd, uint8_t slot, uint16_t* out) {
    if (cmd == nullptr || out == nullptr || cmd->texture == nullptr || cmd->texture->valid == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint16_t bank = (cmd->palette_override != 0u)
        ? cmd->palette_override
        : cmd->texture->palette;
    if (cmd->texture->format == SAT_VDP1_TEXTURE_LUT4) {
        /* The table's entries hold the selector (sat_vdp1_upload_color_calc_lut). */
        if (slot > 7u && slot != SAT_VDP2_SPRITE_ADD) return SAT_ERR_INVALID_ARG;
        *out = bank;
    } else if (cmd->texture->format != SAT_VDP1_TEXTURE_INDEXED8) {
        return SAT_ERR_UNSUPPORTED;
    } else {
        SAT_TRY(saturn::core::vdp1_color_calc::encode_palette_selector(bank, slot, out));
    }
    const sat_result_t claim = sat_vdp2_sprite_color_calc_claim_mode(
        slot == SAT_VDP2_SPRITE_ADD ? SAT_VDP2_COLOR_CALC_ADD : SAT_VDP2_COLOR_CALC_RATIO);
    return claim == SAT_ERR_NOT_INITIALIZED && slot != SAT_VDP2_SPRITE_ADD ? SAT_OK : claim;
}

}  // namespace

extern "C" sat_result_t sat_vdp1_upload_color_calc_lut(
    const uint8_t* codes,
    uint16_t palette_bank,
    uint8_t color_calc_slot,
    uint16_t* out_lut
) {
    if (codes == nullptr || out_lut == nullptr) return SAT_ERR_INVALID_ARG;
    uint16_t selector = 0u;
    SAT_TRY(saturn::core::vdp1_color_calc::encode_palette_selector(palette_bank, color_calc_slot, &selector));
    uint16_t words[16];
    for (uint32_t i = 0u; i < 16u; ++i) {
        words[i] = codes[i] != 0u ? static_cast<uint16_t>((selector << 8u) | codes[i]) : 0u;
    }
    return sat_vdp1_upload_lut(words, out_lut);
}

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
