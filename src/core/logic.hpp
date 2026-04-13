#ifndef SATURN_CORE_LOGIC_HPP
#define SATURN_CORE_LOGIC_HPP

#include "saturn/saturn.h"

namespace saturn::core {

/* ------------------------------------------------------------------ */
/* Pure logic helpers — testable on host with native g++               */
/* ------------------------------------------------------------------ */

inline sat_pad_state_t compute_pad_state(uint16_t prev_held, uint16_t cur_held) {
    sat_pad_state_t state = {};
    state.held = cur_held;
    state.pressed = static_cast<uint16_t>((~prev_held) & cur_held);
    state.released = static_cast<uint16_t>(prev_held & (~cur_held));
    return state;
}

inline sat_result_t validate_video_config(const sat_video_config_t* config) {
    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    /* Accept any resolution; only NTSC is validated here. */
    if (config->ntsc == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }
    return SAT_OK;
}

inline sat_result_t validate_nbg0_config(const sat_vdp2_nbg0_config_t* config) {
    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->char_size > SAT_VDP2_CHAR_SIZE_2X2) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->color_mode > SAT_VDP2_COLOR_MODE_16770000) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->map_plane_index > 0x003Fu) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

inline sat_result_t validate_vdp2_palette_upload(uint16_t count, uint16_t offset) {
    constexpr uint32_t kVdp2CramWordCapacity = 2048u;
    if ((static_cast<uint32_t>(offset) + static_cast<uint32_t>(count)) > kVdp2CramWordCapacity) {
        return SAT_ERR_CAPACITY;
    }
    return SAT_OK;
}

inline sat_result_t validate_vdp2_vram_write(uint32_t offset, uint32_t words) {
    if (words == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    constexpr uint32_t kVdp2VramWordCapacity = (512u * 1024u) / 2u;
    if (offset >= kVdp2VramWordCapacity || (offset + words) > kVdp2VramWordCapacity) {
        return SAT_ERR_CAPACITY;
    }
    return SAT_OK;
}

inline sat_result_t validate_map_region(
    uint16_t region_x,
    uint16_t region_y,
    uint16_t region_w,
    uint16_t region_h,
    uint16_t map_w,
    uint16_t map_h,
    uint16_t stride
) {
    if (region_w == 0u || region_h == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (region_x >= map_w || region_y >= map_h) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((static_cast<uint32_t>(region_x) + static_cast<uint32_t>(region_w)) > map_w) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((static_cast<uint32_t>(region_y) + static_cast<uint32_t>(region_h)) > map_h) {
        return SAT_ERR_INVALID_ARG;
    }
    if (stride < region_w) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

inline uint32_t compute_map_base_words(uint16_t plane_index) {
    return static_cast<uint32_t>(plane_index) << 10u;
}

inline uint32_t compute_map_row_offset(
    uint16_t plane_index,
    uint16_t map_w,
    uint16_t row
) {
    return compute_map_base_words(plane_index) + static_cast<uint32_t>(row) * static_cast<uint32_t>(map_w);
}

/* Resolved sprite data — used internally by sat_draw_sprite */
struct ResolvedSprite {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t srca;
    uint16_t palette;
};

inline sat_result_t resolve_sprite_cmd(
    const sat_sprite_cmd_t* cmd,
    ResolvedSprite* out
) {
    if (cmd == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (cmd->texture == nullptr || cmd->texture->valid == 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    // Coordinates: fx16 (16.16) -> integer pixels
    out->x = static_cast<int16_t>(cmd->x >> 16);
    out->y = static_cast<int16_t>(cmd->y >> 16);
    out->width = (cmd->width != 0u) ? cmd->width : cmd->texture->width;
    out->height = (cmd->height != 0u) ? cmd->height : cmd->texture->height;
    out->srca = cmd->texture->srca;
    out->palette = (cmd->palette_override != 0u) ? cmd->palette_override : cmd->texture->palette;
    return SAT_OK;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_LOGIC_HPP */
