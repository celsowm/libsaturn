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
    /* Accept any resolution; the standard is PAL, NTSC or automatic. */
    if (config->ntsc > SAT_VIDEO_AUTO) {
        return SAT_ERR_INVALID_ARG;
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

/* CRAM holds 2048 words, which is 8 banks of 256 entries, so a palette bank
 * index is only valid in 0..7. hal::vdp1::upload_palette writes
 * CRAM[index * 256 ...] with no bounds check of its own, so bank 8 lands past
 * the end of CRAM -- in practice wrapping onto bank 0 and overwriting whatever
 * palette the VDP2 layers are using. Nothing about that failure is visible at
 * the call site, which is why it is rejected here. */
constexpr uint16_t kPaletteBankCount = 8u;

inline sat_result_t validate_palette_bank(uint16_t palette_index) {
    if (palette_index >= kPaletteBankCount) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

/* VDP1 character pattern limits from the hardware manual (Chapter 5.1):
 * width 8..504 in multiples of 8, height 1..255. Width enforcement already
 * existed in the HAL (non-zero, multiple of 8); the upper bounds are the
 * documented table-size limits. Centralized here so both the combined and
 * the split upload paths agree. */
constexpr uint16_t kVdp1MaxTextureWidth = 504u;
constexpr uint16_t kVdp1MaxTextureHeight = 255u;

inline sat_result_t validate_indexed8_texture_dims(uint16_t width, uint16_t height) {
    if (width == 0u || height == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((width & 7u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (width > kVdp1MaxTextureWidth || height > kVdp1MaxTextureHeight) {
        return SAT_ERR_INVALID_ARG;
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

/* ------------------------------------------------------------------ */
/* VDP1 non-texture command encoding                                   */
/* ------------------------------------------------------------------ */

/* VDP1 command select values (CMDCTRL bits 3..0), VDP1 manual 6.1. */
constexpr uint16_t kVdp1CmdNormalSprite = 0x0000u;
constexpr uint16_t kVdp1CmdScaledSprite = 0x0001u;
constexpr uint16_t kVdp1CmdDistortedSprite = 0x0002u;
constexpr uint16_t kVdp1CmdPolygon  = 0x0004u;  /* filled interior            */
constexpr uint16_t kVdp1CmdPolyline = 0x0005u;  /* outline only, 4 vertices   */
constexpr uint16_t kVdp1CmdLine     = 0x0006u;  /* straight line, 2 vertices  */
constexpr uint16_t kVdp1CmdUserClip = 0x0008u;  /* user clipping coordinates   */
constexpr uint16_t kVdp1CmdEnd      = 0x8000u;  /* end bit (CMDCTRL bit 15)   */

/* PMOD bits common to all non-texture commands (CMDPMOD, VDP1 manual p06_30):
 *   bit 7     = ECD (end code disable)
 *   bit 6     = SPD (transparent pixel disable)
 *     -- neither means anything without character data; both set so no end
 *        or transparent code is ever looked for
 *   bits 5..3 = color mode 000B: CMDCOLR is used as is, an RGB code or a
 *     color-bank code. It is the mode the manual prescribes for untextured
 *     commands, and the only one an 8bpp high-resolution frame buffer takes.
 *   bits 2..0 = color calculation 000B (replace)
 * This used to be 0x0018, which mistook bits 3/4 for SPD/ECD and so selected
 * color mode 011B instead. */
constexpr uint16_t kVdp1PolygonPmod = 0x00C0u;

constexpr uint16_t kVdp1EffectFlags = SAT_SPRITE_FLAG_OPAQUE |
    SAT_SPRITE_FLAG_MESH | SAT_SPRITE_FLAG_HALF_TRANSPARENT |
    SAT_SPRITE_FLAG_HALF_LUMINANCE;
constexpr uint16_t kVdp1HalfFlags =
    SAT_SPRITE_FLAG_HALF_TRANSPARENT | SAT_SPRITE_FLAG_HALF_LUMINANCE;

inline sat_result_t validate_polygon_effects(uint16_t color, uint16_t flags) {
    if ((flags & ~kVdp1EffectFlags) != 0u ||
        (flags & kVdp1HalfFlags) == kVdp1HalfFlags) return SAT_ERR_INVALID_ARG;
    /* RGB code is required for source color calculations; palette/indexed
     * colors cannot be interpreted as RGB by the VDP1 arithmetic unit. */
    if ((flags & kVdp1HalfFlags) != 0u && (color & 0x8000u) == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }
    return SAT_OK;
}

/* Shadow is the one colour calculation a palette sprite can use: the VDP1
 * takes only its shape from the texture and halves the RGB pixels below. */
inline sat_result_t validate_indexed8_sprite_effects(uint16_t flags) {
    if ((flags & ~(kVdp1EffectFlags | SAT_SPRITE_FLAG_SHADOW)) != 0u ||
        (flags & kVdp1HalfFlags) == kVdp1HalfFlags) return SAT_ERR_INVALID_ARG;
    if ((flags & kVdp1HalfFlags) != 0u) return SAT_ERR_UNSUPPORTED;
    return SAT_OK;
}

inline uint16_t color_calculation_bits(uint16_t flags, bool gouraud) {
    if ((flags & SAT_SPRITE_FLAG_HALF_TRANSPARENT) != 0u) {
        return gouraud ? 0x0007u : 0x0003u;
    }
    if ((flags & SAT_SPRITE_FLAG_HALF_LUMINANCE) != 0u) {
        return gouraud ? 0x0006u : 0x0002u;
    }
    return gouraud ? 0x0004u : 0u;
}


/* Composes CMDPMOD for a polygon/polyline/line command.
 * bit 6 = opaque flag, mirrored from SAT_SPRITE_FLAG_OPAQUE for API symmetry.
 */
inline uint16_t compose_polygon_pmod(uint16_t flags) {
    return static_cast<uint16_t>(
        kVdp1PolygonPmod |
        ((flags & SAT_SPRITE_FLAG_MESH) != 0u ? 0x0100u : 0u) |
        color_calculation_bits(flags, false));
}

/* Composes CMDCTRL for a polygon-family command: command select + end bit. */
inline uint16_t compose_polygon_ctrl(uint16_t command_select, bool is_end) {
    return static_cast<uint16_t>(command_select & 0x000Fu) |
           (is_end ? kVdp1CmdEnd : 0x0000u);
}

/* Gouraud shading (VDP1 manual 5.3): color calculation bits 2..0 = 100B, and
 * CMDGRDA holds the table's VRAM address divided by 8. A table entry of
 * 10h per channel means "no change". */
constexpr uint16_t kVdp1ColorCalcGouraud = 0x0004u;
constexpr uint16_t kGouraudNeutral = 0x4210u;

inline uint16_t compose_gouraud_pmod(uint16_t pmod) {
    const uint16_t base_calc = static_cast<uint16_t>(pmod & 0x0007u);
    const uint16_t gouraud_calc = base_calc == 0x0003u ? 0x0007u :
                                  base_calc == 0x0002u ? 0x0006u :
                                  kVdp1ColorCalcGouraud;
    return static_cast<uint16_t>((pmod & ~0x0007u) | gouraud_calc);
}

inline uint16_t gouraud_table_grda(uint32_t area_base_bytes, uint16_t index) {
    return static_cast<uint16_t>((area_base_bytes / 8u) + index);
}

/* PMOD bits common to texture (sprite-family) commands, matching the value
 * used by normal sprites (CMDT spin+PRIV flag) in push_sprite:
 *   bits 5..3 = color mode 100B (16-bit color bank, 256-entry palette)
 *   bit 7     = ECD (end code disable) — code 0 is transparent, not end
 * bit 6 (transparent-pixel disable) is set only for SAT_SPRITE_FLAG_OPAQUE. */
constexpr uint16_t kVdp1SpritePmodBase = 0x00A0u;

/* Composes CMDPMOD for a scaled/distorted sprite. bit 6 = opaque flag,
 * mirrored from SAT_SPRITE_FLAG_OPAQUE, forcing every texel to be drawn. */
/* LUT4 keeps end codes disabled (a 0xF nibble would otherwise end the line)
 * and selects color mode 001B, lookup table. */
constexpr uint16_t kVdp1SpritePmodLut4Base = 0x0088u;

inline uint16_t compose_sprite_pmod(uint16_t flags,
                                    uint16_t format = SAT_VDP1_TEXTURE_INDEXED8) {
    return static_cast<uint16_t>(
        (format == SAT_VDP1_TEXTURE_LUT4 ? kVdp1SpritePmodLut4Base
                                         : kVdp1SpritePmodBase) |
        ((flags & SAT_SPRITE_FLAG_OPAQUE) != 0u ? 0x0040u : 0u) |
        ((flags & SAT_SPRITE_FLAG_MESH) != 0u ? 0x0100u : 0u) |
        ((flags & SAT_SPRITE_FLAG_SHADOW) != 0u ? 0x0001u : 0u));
}

/* Composes CMDCOLR for a sprite-family command. Color mode 100B uses the
 * 16-bit color bank number in bits 15..8, hence the << 8. In lookup-table
 * mode `palette` already is the table address / 8. */
inline uint16_t compose_sprite_colr(uint16_t palette,
                                    uint16_t format = SAT_VDP1_TEXTURE_INDEXED8) {
    if (format == SAT_VDP1_TEXTURE_LUT4) return palette;
    return static_cast<uint16_t>(palette << 8u);
}

inline uint16_t compose_inside_user_clip_pmod(uint16_t pmod, bool enabled) {
    if (!enabled) return pmod;
    return static_cast<uint16_t>((pmod & static_cast<uint16_t>(~0x0200u)) | 0x0400u);
}

/* ------------------------------------------------------------------ */
/* Scaled / distorted sprite resolution                               */
/* ------------------------------------------------------------------ */

struct ResolvedScaledSprite {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    uint16_t width;   /* source size, becomes CMDSIZE */
    uint16_t height;
    uint16_t srca;
    uint16_t palette;
    uint16_t flags;
    uint16_t format;
};

inline sat_result_t resolve_scaled_sprite_cmd(
    const sat_scaled_sprite_cmd_t* cmd,
    ResolvedScaledSprite* out
) {
    if (cmd == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (cmd->texture == nullptr || cmd->texture->valid == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    out->x0 = cmd->x0;
    out->y0 = cmd->y0;
    out->x1 = cmd->x1;
    out->y1 = cmd->y1;
    out->width = cmd->texture->width;
    out->height = cmd->texture->height;
    out->srca = cmd->texture->srca;
    out->palette = (cmd->palette_override != 0u) ? cmd->palette_override : cmd->texture->palette;
    out->flags = cmd->flags;
    out->format = cmd->texture->format;
    return SAT_OK;
}

struct ResolvedDistortedSprite {
    int16_t x[4];
    int16_t y[4];
    uint16_t width;
    uint16_t height;
    uint16_t srca;
    uint16_t palette;
    uint16_t flags;
    uint16_t format;
};

inline sat_result_t resolve_distorted_sprite_cmd(
    const sat_distorted_sprite_cmd_t* cmd,
    ResolvedDistortedSprite* out
) {
    if (cmd == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (cmd->texture == nullptr || cmd->texture->valid == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 4; ++i) {
        out->x[i] = cmd->x[i];
        out->y[i] = cmd->y[i];
    }
    out->width = cmd->texture->width;
    out->height = cmd->texture->height;
    out->srca = cmd->texture->srca;
    out->palette = (cmd->palette_override != 0u) ? cmd->palette_override : cmd->texture->palette;
    out->flags = cmd->flags;
    out->format = cmd->texture->format;
    return SAT_OK;
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
    uint16_t format;
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
    out->format = cmd->texture->format;
    return SAT_OK;
}

/* ------------------------------------------------------------------ */
/* Layer priority                                                      */
/* ------------------------------------------------------------------ */
/* Every layer, plus the sprite layer that the VDP1 draws into, carries a
 * 3-bit priority. The highest number wins; zero means "never shown", which
 * is a real trap because it looks exactly like a layer that failed to
 * initialise. Ties go to a fixed hardware order, so a background meant to
 * sit behind the VDP1 has to be strictly lower, not equal. */
inline uint16_t compose_nbg0_priority(uint16_t prina, uint8_t priority) {
    return static_cast<uint16_t>((prina & 0xFFF8u) | (priority & 0x07u));
}

/* NBG0 and RBG0 share BGON.  Layer setup must only touch its own enable and
 * transparent-code bits or initializing the sky after the ground makes the
 * ground disappear (and vice versa). */
inline uint16_t compose_nbg0_bgon(uint16_t bgon, bool enabled, bool transparent_code_enabled) {
    bgon = enabled ? static_cast<uint16_t>(bgon | 0x0001u)
                   : static_cast<uint16_t>(bgon & static_cast<uint16_t>(~0x0001u));
    return transparent_code_enabled
        ? static_cast<uint16_t>(bgon & static_cast<uint16_t>(~0x0100u))
        : static_cast<uint16_t>(bgon | 0x0100u);
}

inline uint16_t compose_rbg0_bgon(uint16_t bgon, bool enabled, bool transparent_code_enabled) {
    bgon = enabled ? static_cast<uint16_t>(bgon | 0x0010u)
                   : static_cast<uint16_t>(bgon & static_cast<uint16_t>(~0x0010u));
    return transparent_code_enabled
        ? static_cast<uint16_t>(bgon & static_cast<uint16_t>(~0x1000u))
        : static_cast<uint16_t>(bgon | 0x1000u);
}

/* PRISA holds two sprite priorities, for sprite types that select between
 * them per pixel. The library never uses that, so both halves are set to the
 * same value -- otherwise the effective priority would depend on a sprite
 * type bit the caller did not ask about. */
inline uint16_t compose_sprite_priority(uint8_t priority) {
    const uint16_t p = static_cast<uint16_t>(priority & 0x07u);
    return static_cast<uint16_t>(p | (p << 8u));
}

/* ------------------------------------------------------------------ */
/* Tiled image upload                                                  */
/* ------------------------------------------------------------------ */
/* NBG0 in cell format reads 8x8 tiles through a map of pattern names. An
 * ordinary linear image therefore has to be cut into tiles, and a map built
 * that puts them back in order -- which is the whole of what these two
 * helpers do. */
constexpr uint16_t kVdp2CellPx = 8u;
constexpr uint16_t kVdp2CellBytes = 64u;   /* 8x8 at 8bpp */
constexpr uint16_t kVdp2CellWords = 32u;
constexpr uint16_t kVdp2MapCells = 64u;    /* one 64x64-cell plane */

/* A 1-word pattern name addresses character data in 32-byte units, so a
 * 64-byte 8bpp cell advances the character number by two. */
constexpr uint32_t kVdp2CharNumberUnitBytes = 32u;

/* The 12-bit character number in a 1-word pattern name cannot reach all of
 * VRAM on its own; supplementary bits in PNCN0 supply the top of the address.
 * With the register sequence sat_vdp2_nbg0_init writes, the reachable window
 * is in VRAM bank B1, and character data has to start 0x2000 bytes into it:
 *
 *     byte address = 0x62000 + ((character_number - 0x100) * 32)
 *
 * The 0x2000 offset is EMPIRICAL, not derived. Cells written at 0x60000 and
 * addressed from character number 0 render as garbage on Ymir, while the
 * same cells at 0x62000 addressed from 0x100 render correctly -- verified by
 * uploading a structured texture and comparing the screenshot against the
 * source image pixel for pixel. Why the first 0x2000 bytes do not work was
 * not established, so this is a measured constant rather than an explained
 * one; do not "simplify" it back to zero without re-running that test.
 *
 * What IS confirmed is that the rest of the window behaves linearly: a
 * 256x256 image (1024 cells, character numbers up to 0x8FE) renders 1:1,
 * so the character number really is 12 bits wide here. */
constexpr uint32_t kVdp2CellWindowByteBase = 0x62000u;
constexpr uint32_t kVdp2CellWindowWordBase = kVdp2CellWindowByteBase / 2u;
constexpr uint16_t kVdp2FirstCharNumber = 0x100u;
constexpr uint32_t kVdp2MaxCharNumber = 0x0FFFu;

/* The map plane sits in the same window, above the character data, so the
 * cells have to stop before it. One page of 1-word pattern names for a
 * 64x64-cell plane is 0x1000 words. */
inline uint32_t nbg0_map_word_base(uint16_t plane_index) {
    return static_cast<uint32_t>(plane_index) << 12u;
}

inline sat_result_t validate_nbg0_image(uint16_t width, uint16_t height, uint16_t plane_index) {
    if (width == 0u || height == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((width % kVdp2CellPx) != 0u || (height % kVdp2CellPx) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    /* The image repeats across the plane, so it may be smaller than the
     * plane -- but a tile the map cannot address is a silent truncation. */
    if ((width / kVdp2CellPx) > kVdp2MapCells || (height / kVdp2CellPx) > kVdp2MapCells) {
        return SAT_ERR_CAPACITY;
    }

    const uint32_t tiles = static_cast<uint32_t>(width / kVdp2CellPx) *
                           static_cast<uint32_t>(height / kVdp2CellPx);
    const uint32_t cell_words = tiles * kVdp2CellWords;
    const uint32_t map_base = nbg0_map_word_base(plane_index);
    if (map_base <= kVdp2CellWindowWordBase) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((kVdp2CellWindowWordBase + cell_words) > map_base) {
        return SAT_ERR_CAPACITY;
    }
    if ((kVdp2FirstCharNumber + ((tiles - 1u) * (kVdp2CellBytes / kVdp2CharNumberUnitBytes))) >
        kVdp2MaxCharNumber) {
        return SAT_ERR_CAPACITY;
    }
    return SAT_OK;
}

/* One 8x8 cell, taken out of a linear indexed8 image.
 *
 * Row-major, left to right and top to bottom, which is the layout the VDP2
 * reads character data in. (JoEngine writes cells transposed and mirrored
 * and then transposes the map to match; the two cancel out to a 90-degree
 * rotation of the whole image, which nobody notices on a floor texture. It
 * is not what the hardware wants.) */
inline void build_cell_indexed8(
    const uint8_t* pixels,
    uint16_t image_width,
    uint16_t tile_x,
    uint16_t tile_y,
    uint8_t* out_cell
) {
    for (uint16_t row = 0; row < kVdp2CellPx; ++row) {
        const uint32_t src_y = (static_cast<uint32_t>(tile_y) * kVdp2CellPx) + row;
        const uint32_t src_x = static_cast<uint32_t>(tile_x) * kVdp2CellPx;
        for (uint16_t col = 0; col < kVdp2CellPx; ++col) {
            out_cell[(row * kVdp2CellPx) + col] =
                pixels[(src_y * image_width) + src_x + col];
        }
    }
}

/* Pattern name for the tile at (tile_x, tile_y) of an image whose cells were
 * written consecutively, row-major, from the base of the character window. */
inline uint16_t compose_pattern_name(
    uint16_t palette_id,
    uint16_t tiles_x,
    uint16_t tile_x,
    uint16_t tile_y
) {
    const uint32_t cell_index = (static_cast<uint32_t>(tile_y) * tiles_x) + tile_x;
    const uint32_t character = kVdp2FirstCharNumber +
                               (cell_index * (kVdp2CellBytes / kVdp2CharNumberUnitBytes));
    return static_cast<uint16_t>(((palette_id & 0x0Fu) << 12u) |
                                 static_cast<uint16_t>(character & kVdp2MaxCharNumber));
}

}  // namespace saturn::core

#endif /* SATURN_CORE_LOGIC_HPP */
