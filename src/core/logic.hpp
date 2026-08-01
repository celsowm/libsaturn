#ifndef SATURN_CORE_LOGIC_HPP
#define SATURN_CORE_LOGIC_HPP

#include "saturn/saturn.h"

namespace saturn::core {

constexpr uint32_t kVdp2VramWordCapacity = (512u * 1024u) / 2u;
constexpr uint32_t kRbg0RotationParamWordCount = 48u;

/* Rotation parameter table word offsets (16-bit word indices from table base).
 * Matches ST-013-R3-061694 byte layout; divide each byte offset by 2.
 *
 *  Word 0-1  : Xst integer / fraction       (bytes 0x00-0x03)
 *  Word 2-3  : Yst integer / fraction       (bytes 0x04-0x07)
 *  Word 4-5  : Zst integer / fraction       (bytes 0x08-0x0B)
 *  Word 6-7  : DeltaXst integer / fraction  (bytes 0x0C-0x0F)
 *  Word 8-9  : DeltaYst integer / fraction  (bytes 0x10-0x13)
 *  Word 10-11: DeltaX  integer / fraction   (bytes 0x14-0x17)
 *  Word 12-13: DeltaY integer / fraction    (bytes 0x18-0x1B)
 *  Word 14-25: Rotation matrix A-F          (bytes 0x1C-0x2F, 6 x 32-bit fx16)
 *  Word 26-29: Viewpoint Px, Py, Pz (+pad)  (bytes 0x34-0x3B, +reserved at 0x36)
 *  Word 30-33: Center Cx, Cy, Cz (+pad)     (bytes 0x38-0x3F, +reserved at 0x3E)
 *  Word 34-37: Parallel move Mx, My         (bytes 0x40-0x47)
 *  Word 38-41: Scaling Kx, Ky               (bytes 0x48-0x4F)
 *  Word 42-43: Kast                         (bytes 0x50-0x53)
 *  Word 44-45: DeltaKast                    (bytes 0x54-0x57)
 *  Word 46-47: DeltaKax                     (bytes 0x58-0x5B)
 *  Word 48+  : (padding to reach 48 words)
 */
constexpr uint32_t kRbg0ScrollWordOffset       = 0u;
constexpr uint32_t kRbg0ScrollFracWordOffset   = 1u;
constexpr uint32_t kRbg0ScrollYWordOffset      = 2u;
constexpr uint32_t kRbg0ScrollYFracWordOffset  = 3u;
constexpr uint32_t kRbg0ScrollZWordOffset      = 4u;   /* Zst integer   (byte 0x08) */
constexpr uint32_t kRbg0ScrollZFracWordOffset  = 5u;   /* Zst fraction  (byte 0x0A) */
constexpr uint32_t kRbg0DScrollWordOffset      = 6u;   /* ΔXst integer  (byte 0x0C) */
constexpr uint32_t kRbg0DScrollFracWordOffset  = 7u;   /* ΔXst fraction (byte 0x0E) */
constexpr uint32_t kRbg0DScrollYWordOffset     = 8u;   /* ΔYst integer  (byte 0x10) */
constexpr uint32_t kRbg0DScrollYFracWordOffset = 9u;   /* ΔYst fraction (byte 0x12) */
constexpr uint32_t kRbg0DotStepWordOffset      = 10u;  /* ΔX integer    (byte 0x14) */
constexpr uint32_t kRbg0DotStepFracWordOffset  = 11u;  /* ΔX fraction   (byte 0x16) */
constexpr uint32_t kRbg0DotStepYWordOffset     = 12u;  /* ΔY integer    (byte 0x18) */
constexpr uint32_t kRbg0DotStepYFracWordOffset = 13u;  /* ΔY fraction   (byte 0x1A) */
constexpr uint32_t kRbg0MatrixWordOffset       = 14u;  /* Matrix A-F    (byte 0x1C) */
constexpr uint32_t kRbg0ViewpointWordOffset    = 26u;  /* Px,Py,Pz      (byte 0x34) */
constexpr uint32_t kRbg0CenterWordOffset       = 30u;  /* Cx,Cy,Cz      (byte 0x38) */
constexpr uint32_t kRbg0ParallelMoveWordOffset = 34u;  /* Mx,My         (byte 0x40) */
constexpr uint32_t kRbg0ScalingWordOffset      = 38u;  /* Kx,Ky         (byte 0x48) */
constexpr uint32_t kRbg0KastWordOffset         = 42u;  /* Kast          (byte 0x50) */
constexpr uint32_t kRbg0DeltaKastWordOffset    = 44u;  /* ΔKast         (byte 0x54) */
constexpr uint32_t kRbg0DeltaKaxWordOffset     = 46u;  /* ΔKax          (byte 0x58) */

/* ------------------------------------------------------------------ */
/* Pure logic helpers — testable on host with native g++               */
/* ------------------------------------------------------------------ */

/* fx16 (16.16 fixed point) -> integer, truncating toward negative infinity
 * (arithmetic right shift). Matches the conversion inlined in resolve_sprite_cmd.
 */
inline int32_t fx16_to_int_impl(sat_fx16_t v) {
    return static_cast<int32_t>(v >> 16);
}

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
    const uint64_t end = static_cast<uint64_t>(offset) + static_cast<uint64_t>(count);
    if (end > kVdp2CramWordCapacity) {
        return SAT_ERR_CAPACITY;
    }
    return SAT_OK;
}

inline sat_result_t validate_vdp2_vram_write(uint32_t offset, uint32_t words) {
    if (words == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint64_t end = static_cast<uint64_t>(offset) + static_cast<uint64_t>(words);
    if (offset >= kVdp2VramWordCapacity || end > kVdp2VramWordCapacity) {
        return SAT_ERR_CAPACITY;
    }
    return SAT_OK;
}

inline bool is_supported_rbg0_bitmap_size(sat_vdp2_rbg0_bitmap_size_t bitmap_size) {
    return bitmap_size == SAT_VDP2_RBG0_BITMAP_512x256 ||
           bitmap_size == SAT_VDP2_RBG0_BITMAP_512x512;
}

inline uint32_t rbg0_bitmap_word_size(sat_vdp2_rbg0_bitmap_size_t bitmap_size) {
    switch (bitmap_size) {
    case SAT_VDP2_RBG0_BITMAP_512x256:
        return (512u * 256u) / 2u;
    case SAT_VDP2_RBG0_BITMAP_512x512:
        return (512u * 512u) / 2u;
    default:
        return 0u;
    }
}

inline uint16_t compose_rbg0_bitmap_control_word(
    sat_vdp2_color_mode_t color_mode,
    sat_vdp2_rbg0_bitmap_size_t bitmap_size
) {
    uint16_t value = static_cast<uint16_t>((static_cast<uint16_t>(color_mode) & 0x0007u) << 12u);
    value = static_cast<uint16_t>(value | 0x0200u);  // R0BMEN
    if (bitmap_size == SAT_VDP2_RBG0_BITMAP_512x512) {
        value = static_cast<uint16_t>(value | 0x0400u);  // R0BMSZ
    }
    return value;
}

inline sat_result_t validate_rbg0_config(const sat_vdp2_rbg0_config_t* config) {
    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->color_mode > SAT_VDP2_COLOR_MODE_16770000) {
        return SAT_ERR_INVALID_ARG;
    }
    if (!is_supported_rbg0_bitmap_size(static_cast<sat_vdp2_rbg0_bitmap_size_t>(config->bitmap_size))) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t bitmap_words = rbg0_bitmap_word_size(
        static_cast<sat_vdp2_rbg0_bitmap_size_t>(config->bitmap_size));
    if (bitmap_words == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((config->bitmap_base_word & 0xFFFFu) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (validate_vdp2_vram_write(config->bitmap_base_word, bitmap_words) != SAT_OK) {
        return SAT_ERR_CAPACITY;
    }
    if (validate_vdp2_vram_write(config->rot_param_base_word, kRbg0RotationParamWordCount) != SAT_OK) {
        return SAT_ERR_CAPACITY;
    }
    return SAT_OK;
}

inline sat_result_t validate_rbg0_rotation_table_offset(uint32_t rot_param_word_offset) {
    return validate_vdp2_vram_write(rot_param_word_offset, kRbg0RotationParamWordCount);
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
    /* For NBG0 cell format with 1-word pattern names and 1x1 characters,
     * one page is 0x2000 bytes = 0x1000 words.
     */
    return static_cast<uint32_t>(plane_index) << 12u;
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
    uint16_t flags;
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
    out->x = static_cast<int16_t>(fx16_to_int_impl(cmd->x));
    out->y = static_cast<int16_t>(fx16_to_int_impl(cmd->y));
    out->width = (cmd->width != 0u) ? cmd->width : cmd->texture->width;
    out->height = (cmd->height != 0u) ? cmd->height : cmd->texture->height;
    out->srca = cmd->texture->srca;
    out->palette = (cmd->palette_override != 0u) ? cmd->palette_override : cmd->texture->palette;
    out->flags = cmd->flags;
    return SAT_OK;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_LOGIC_HPP */
