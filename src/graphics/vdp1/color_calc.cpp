#include "saturn/vdp1_color_calc.h"

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
    return saturn::core::vdp1_color_calc::encode_palette_selector(bank, slot, out);
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
