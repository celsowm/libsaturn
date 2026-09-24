#include "src/hal/vdp1/vdp1.hpp"
#include "src/core/runtime/logic.hpp"
#include "saturn/vdp1.h"
#include "src/graphics/3d/scene/test_metrics.h"

#ifndef SAT_PROFILE_METRICS
#define SAT_PROFILE_METRICS 0
#endif

namespace saturn::hal::vdp1 {

namespace {

constexpr uintptr_t kUncached = 0x20000000u;

/* These are macros rather than reference variables on purpose: a reference or
 * pointer bound to a reinterpret_cast is dynamically initialized, and this
 * build runs no static constructors (crt0.s calls _main directly and the
 * linker script has no .init_array pass). GCC constant-folds most of them at
 * -O2, but the ones it does not silently become null and every access reads or
 * writes address zero. See the long explanation in src/hal/vdp2/vdp2.cpp and the
 * build-time guard in tools/check_no_static_ctors.py. */
#define TVMR (*reinterpret_cast<volatile uint16_t*>(kUncached | 0x05D00000u))
#define FBCR (*reinterpret_cast<volatile uint16_t*>(kUncached | 0x05D00002u))
#define PTMR (*reinterpret_cast<volatile uint16_t*>(kUncached | 0x05D00004u))
#define EWDR (*reinterpret_cast<volatile uint16_t*>(kUncached | 0x05D00006u))
#define EWLR (*reinterpret_cast<volatile uint16_t*>(kUncached | 0x05D00008u))
#define EWRR (*reinterpret_cast<volatile uint16_t*>(kUncached | 0x05D0000Au))
/* These are macros rather than reference variables on purpose: a reference or
 * pointer bound to a reinterpret_cast is dynamically initialized, and this
 * build runs no static constructors (crt0.s calls _main directly and the
 * linker script has no .init_array pass). GCC constant-folds most of them at
 * -O2, but the ones it does not silently become null and every access reads or
 * writes address zero. See the long explanation in src/hal/vdp2/vdp2.cpp and the
 * build-time guard in tools/check_no_static_ctors.py. */
#define VDP2_CRAM (reinterpret_cast<volatile uint16_t*>(kUncached | 0x05F00000u))
#define VDP1_VRAM_32 (reinterpret_cast<volatile uint32_t*>(kUncached | 0x05C00000u))
#define VDP1_VRAM_16 (reinterpret_cast<volatile uint16_t*>(kUncached | 0x05C00000u))
constexpr uint32_t kVramSize = 512u * 1024u;
/* Room for saturn::internal::kCmdCapacity 32-byte command tables; texture
 * uploads start right after it. */
constexpr uint32_t kCommandAreaBytes = 64u * 1024u;
/* Gouraud shading tables (VDP1 manual 5.3), 8 bytes each, right after the
 * command area. Like commands they are staged in work RAM during the frame
 * and copied to VRAM in submit(); textures start after both areas. */
constexpr uint32_t kGouraudAreaBytes = 16u * 1024u;
constexpr uint16_t kGouraudTableCapacity = static_cast<uint16_t>(kGouraudAreaBytes / 8u);
constexpr uint32_t kTextureBase = kCommandAreaBytes + kGouraudAreaBytes;

Command* g_cmd_buffer = nullptr;
uint16_t g_cmd_capacity = 0;
uint16_t g_cmd_count = 0;
uint16_t g_overlay_reserved = 0;
bool g_overlay_pass = false;
uint16_t g_gouraud_words[kGouraudTableCapacity * 4u];
uint16_t g_gouraud_count = 0;
uint32_t g_frame_serial=0u;
bool g_frame_submitted=true;
uint32_t g_texture_cursor = kTextureBase;
uint16_t g_width = 320;
uint16_t g_height = 224;
#if SAT_PROFILE_METRICS
uint32_t g_test_scene_command_hash;
uint32_t g_test_scene_command_count;

void hash_command_word(uint32_t& hash, uint16_t word) {
    hash ^= static_cast<uint8_t>(word >> 8u);
    hash *= 16777619u;
    hash ^= static_cast<uint8_t>(word);
    hash *= 16777619u;
}

void capture_scene_commands() {
    uint32_t hash = 2166136261u;
    g_test_scene_command_count = g_cmd_count;
    hash_command_word(hash, g_cmd_count);
    for (uint16_t i = 0u; i < g_cmd_count; ++i) {
        const Command& c = g_cmd_buffer[i];
        hash_command_word(hash,c.ctrl); hash_command_word(hash,c.link);
        hash_command_word(hash,c.pmod); hash_command_word(hash,c.colr);
        hash_command_word(hash,c.srca); hash_command_word(hash,c.size);
        hash_command_word(hash,static_cast<uint16_t>(c.xa));
        hash_command_word(hash,static_cast<uint16_t>(c.ya));
        hash_command_word(hash,static_cast<uint16_t>(c.xb));
        hash_command_word(hash,static_cast<uint16_t>(c.yb));
        hash_command_word(hash,static_cast<uint16_t>(c.xc));
        hash_command_word(hash,static_cast<uint16_t>(c.yc));
        hash_command_word(hash,static_cast<uint16_t>(c.xd));
        hash_command_word(hash,static_cast<uint16_t>(c.yd));
        hash_command_word(hash,c.grda);
    }
    hash_command_word(hash,g_gouraud_count);
    for (uint32_t i = 0u; i < static_cast<uint32_t>(g_gouraud_count) * 4u; ++i)
        hash_command_word(hash,g_gouraud_words[i]);
    g_test_scene_command_hash = hash;
}
#endif

/* One terminator command is always unavailable to user draw calls. During
 * the world pass, hold additional entries in reserve for the final HUD.
 * A single common check protects EVERY primitive type, including Gouraud
 * whose command is allocated by push_polygon_like. */
inline bool command_slot_unavailable() {
    return static_cast<uint32_t>(g_cmd_count) + 1u +
        (g_overlay_pass ? 0u : g_overlay_reserved) >= g_cmd_capacity;
}

inline void copy_words_to_vram(const Command* src, uint32_t count_commands) {
    const uint32_t dwords = (count_commands * sizeof(Command)) / sizeof(uint32_t);
    const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
    for (uint32_t i = 0; i < dwords; ++i) {
        VDP1_VRAM_32[i] = s[i];
    }
}

}  // namespace

void init(uint16_t width, uint16_t height, uint16_t /* clear_color */) {
    g_width = width;
    g_height = height;
    g_texture_cursor = kTextureBase;

    TVMR = 0x0000;
    FBCR = 0x0000;
    PTMR = 0x0000;
    // Transparent erase. In the 16bpp framebuffer bit15 = 1 is the RGB code,
    // i.e. an OPAQUE pixel; 0x0000 is the "no data" code that lets the VDP2
    // backdrop and lower VDP2 layers show through. See
    // docs/sega_saturn_hardware/hard/vdp1/hon/p04_14.md.
    // clear_color is ignored here because the default erase is transparent;
    // call set_clear_color() for an opaque erase instead.
    EWDR = 0x0000u;
    // Erase enabled on entire screen to clear VDP1 framebuffer each frame.
    // EWRR bit15..9 = X3 (units of 8 px, actual X3 = value*8 - 1),
    // bit8..0 = Y3, the last erased line, hence height - 1.
    EWLR = 0x0000;
    EWRR = static_cast<uint16_t>(((width / 8u) << 9u) | (height - 1u));

    VDP1_VRAM_32[0] = 0x80000000u;
    VDP1_VRAM_32[1] = 0x00000000u;
    VDP1_VRAM_32[2] = 0x00000000u;
    VDP1_VRAM_32[3] = 0x00000000u;
    PTMR = 0x0002;
}

void set_clear_color(uint16_t rgb555) {
    // Sets bit 15, the RGB code, producing an OPAQUE erase in the given color.
    // This covers the VDP2 backdrop and any VDP2 layer below the sprite layer.
    // Use set_erase_transparent() to restore the see-through default.
    EWDR = static_cast<uint16_t>(rgb555 | 0x8000u);
}

void set_erase_transparent() {
    // 0x0000 is the framebuffer "no data" code: nothing is drawn, so the VDP2
    // backdrop and lower VDP2 layers remain visible.
    EWDR = 0x0000u;
}

void set_erase_enabled(bool enable, uint16_t width, uint16_t height) {
    if (enable) {
        EWLR = 0x0000;
        EWRR = static_cast<uint16_t>(((width / 8u) << 9u) | (height - 1u));
    } else {
        EWLR = 0x0000;
        EWRR = 0x0000;
    }
}

void begin_frame(Command* command_buffer, uint16_t capacity) {
    ++g_frame_serial;
    if(g_frame_serial==0u)g_frame_serial=1u;
    g_frame_submitted=false;
    g_cmd_buffer = command_buffer;
    g_cmd_capacity = capacity;
    g_cmd_count = 0;
    g_overlay_reserved = 0u;
    g_overlay_pass = false;
    g_gouraud_count = 0;

    // Both the local (relative) coordinate register and the system clipping
    // register are hardware-undefined after power-on/reset (VDP1 manual
    // 7.1/7.3) and persist only until explicitly rewritten. Every command
    // list submitted here starts fresh at VRAM word 0 (see submit()'s
    // copy_words_to_vram call), so without re-issuing these two setup
    // commands every frame, sprite coordinates silently stop being relative
    // to screen center (0,0) as every other function in this HAL assumes,
    // and drift onto whatever garbage the clip/offset registers held.
    if (g_cmd_capacity < 2u) {
        return;
    }

    Command& local_coord = g_cmd_buffer[g_cmd_count++];
    local_coord.ctrl = saturn::core::compose_polygon_ctrl(0x000Au, false);  // 1010B: local coordinate set
    local_coord.link = 0;
    local_coord.pmod = 0;
    local_coord.colr = 0;
    local_coord.srca = 0;
    local_coord.size = 0;
    local_coord.xa = static_cast<int16_t>(g_width / 2u);
    local_coord.ya = static_cast<int16_t>(g_height / 2u);
    local_coord.xb = 0;
    local_coord.yb = 0;
    local_coord.xc = 0;
    local_coord.yc = 0;
    local_coord.xd = 0;
    local_coord.yd = 0;
    local_coord.grda = 0;
    local_coord.pad = 0;

    Command& sys_clip = g_cmd_buffer[g_cmd_count++];
    sys_clip.ctrl = saturn::core::compose_polygon_ctrl(0x0009u, false);  // 1001B: system clipping coordinate set
    sys_clip.link = 0;
    sys_clip.pmod = 0;
    sys_clip.colr = 0;
    sys_clip.srca = 0;
    sys_clip.size = 0;
    sys_clip.xa = 0;
    sys_clip.ya = 0;
    sys_clip.xb = 0;
    sys_clip.yb = 0;
    sys_clip.xc = static_cast<int16_t>(g_width - 1u);
    sys_clip.yc = static_cast<int16_t>(g_height - 1u);
    sys_clip.xd = 0;
    sys_clip.yd = 0;
    sys_clip.grda = 0;
    sys_clip.pad = 0;
}

void command_stats(uint16_t& used, uint16_t& capacity,
                   uint16_t& overlay_reserved, bool& overlay_pass) {
    used = g_cmd_count;
    capacity = g_cmd_capacity;
    overlay_reserved = g_overlay_reserved;
    overlay_pass = g_overlay_pass;
}

sat_result_t command_checkpoint(sat_vdp1_command_checkpoint_t* out) {
    if(!out) return SAT_ERR_INVALID_ARG;
    if(!g_cmd_buffer || g_frame_submitted)return SAT_ERR_NOT_INITIALIZED;
    out->frame_serial=g_frame_serial;
    out->used=g_cmd_count;
    out->gouraud_tables=g_gouraud_count;
    out->overlay_reserved=g_overlay_reserved;
    out->overlay_pass=g_overlay_pass?1u:0u;
    out->reserved=0u;
    return SAT_OK;
}

sat_result_t command_rollback(
    const sat_vdp1_command_checkpoint_t* checkpoint) {
    if(!checkpoint)return SAT_ERR_INVALID_ARG;
    if(!g_cmd_buffer || g_frame_submitted || !g_frame_serial ||
       checkpoint->frame_serial!=g_frame_serial ||
       checkpoint->used>g_cmd_count ||
       checkpoint->gouraud_tables>g_gouraud_count ||
       checkpoint->overlay_reserved!=g_overlay_reserved ||
       checkpoint->overlay_pass!=(g_overlay_pass?1u:0u))
        return SAT_ERR_INVALID_ARG;
    /* All draw calls append commands and Gouraud words to WORK RAM. The
     * later submit() is the only path writing them to VDP1 VRAM. */
    g_cmd_count=checkpoint->used;
    g_gouraud_count=checkpoint->gouraud_tables;
    return SAT_OK;
}

sat_result_t reserve_overlay_commands(uint16_t count) {
    if(g_cmd_buffer == nullptr) return SAT_ERR_NOT_INITIALIZED;
    if(g_overlay_pass) return SAT_ERR_UNSUPPORTED;
    if(static_cast<uint32_t>(g_cmd_count)+1u+count>g_cmd_capacity)
        return SAT_ERR_CAPACITY;
    g_overlay_reserved=count;
    return SAT_OK;
}

sat_result_t begin_overlay_pass() {
    if(g_cmd_buffer == nullptr) return SAT_ERR_NOT_INITIALIZED;
#if SAT_PROFILE_METRICS
    /* Hash world commands before diagnostic/game HUD additions. */
    capture_scene_commands();
#endif
    g_overlay_pass=true;
    return SAT_OK;
}

sat_result_t push_user_clip(const UserClipRequest& req) {
    if (g_cmd_buffer == nullptr) return SAT_ERR_NOT_INITIALIZED;
    if (req.x0 > req.x1 || req.y0 > req.y1 ||
        req.x1 >= g_width || req.y1 >= g_height) {
        return SAT_ERR_INVALID_ARG;
    }
    if (command_slot_unavailable()) return SAT_ERR_CAPACITY;

    Command& cmd = g_cmd_buffer[g_cmd_count++];
    cmd.ctrl = saturn::core::compose_polygon_ctrl(saturn::core::kVdp1CmdUserClip, false);
    cmd.link = 0;
    cmd.pmod = 0;
    cmd.colr = 0;
    cmd.srca = 0;
    cmd.size = 0;
    cmd.xa = static_cast<int16_t>(req.x0);
    cmd.ya = static_cast<int16_t>(req.y0);
    cmd.xb = 0;
    cmd.yb = 0;
    cmd.xc = static_cast<int16_t>(req.x1);
    cmd.yc = static_cast<int16_t>(req.y1);
    cmd.xd = 0;
    cmd.yd = 0;
    cmd.grda = 0;
    cmd.pad = 0;
    return SAT_OK;
}

sat_result_t push_sprite(const SpriteRequest& req) {
    if (g_cmd_buffer == nullptr) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    SAT_TRY(saturn::core::validate_indexed8_sprite_effects(req.flags));
    if (req.width == 0 || req.height == 0) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((req.width & 7u) != 0) {
        return SAT_ERR_INVALID_ARG;
    }
    if (command_slot_unavailable()) {
        return SAT_ERR_CAPACITY;
    }

    Command& cmd = g_cmd_buffer[g_cmd_count++];
    cmd.ctrl = 0x0000;
    cmd.link = 0;
    cmd.pmod = saturn::core::compose_inside_user_clip_pmod(
        saturn::core::compose_sprite_pmod(req.flags, req.format), req.user_clip);
    cmd.colr = saturn::core::compose_sprite_colr(req.palette, req.format);
    cmd.srca = req.srca;
    cmd.size = static_cast<uint16_t>(((req.width / 8u) << 8u) | req.height);
    // VDP1 sprite corners: coordinates are in pixels (0 = center of screen)
    cmd.xa = req.x;
    cmd.ya = req.y;
    cmd.xb = static_cast<int16_t>(req.x + req.width);
    cmd.yb = req.y;
    cmd.xc = static_cast<int16_t>(req.x + req.width);
    cmd.yc = static_cast<int16_t>(req.y + req.height);
    cmd.xd = req.x;
    cmd.yd = static_cast<int16_t>(req.y + req.height);
    cmd.grda = 0;
    cmd.pad = 0;
    return SAT_OK;
}

sat_result_t push_scaled_sprite(const ScaledSpriteRequest& req) {
    if (g_cmd_buffer == nullptr) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    SAT_TRY(saturn::core::validate_indexed8_sprite_effects(req.flags));
    if (req.width == 0u || req.height == 0u || (req.width & 7u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (command_slot_unavailable()) {
        return SAT_ERR_CAPACITY;
    }

    Command& cmd = g_cmd_buffer[g_cmd_count++];
    cmd.ctrl = saturn::core::compose_polygon_ctrl(saturn::core::kVdp1CmdScaledSprite, false);
    cmd.link = 0;
    cmd.pmod = saturn::core::compose_inside_user_clip_pmod(
        saturn::core::compose_sprite_pmod(req.flags, req.format), req.user_clip);
    cmd.colr = saturn::core::compose_sprite_colr(req.palette, req.format);
    cmd.srca = req.srca;
    cmd.size = static_cast<uint16_t>(((req.width / 8u) << 8u) | req.height);
    /* Two-coordinate rectangle: A top-left, C bottom-right. B and D are filled
     * consistently even though the hardware only reads A/C in this form. */
    cmd.xa = req.x0;
    cmd.ya = req.y0;
    cmd.xb = req.x1;
    cmd.yb = req.y0;
    cmd.xc = req.x1;
    cmd.yc = req.y1;
    cmd.xd = req.x0;
    cmd.yd = req.y1;
    cmd.grda = 0;
    cmd.pad = 0;
    return SAT_OK;
}

sat_result_t push_distorted_sprite(const DistortedSpriteRequest& req) {
    if (g_cmd_buffer == nullptr) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    SAT_TRY(saturn::core::validate_indexed8_sprite_effects(req.flags));
    if (req.width == 0u || req.height == 0u || (req.width & 7u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (command_slot_unavailable()) {
        return SAT_ERR_CAPACITY;
    }

    Command& cmd = g_cmd_buffer[g_cmd_count++];
    cmd.ctrl = saturn::core::compose_polygon_ctrl(saturn::core::kVdp1CmdDistortedSprite, false);
    cmd.link = 0;
    cmd.pmod = saturn::core::compose_inside_user_clip_pmod(
        saturn::core::compose_sprite_pmod(req.flags, req.format), req.user_clip);
    cmd.colr = saturn::core::compose_sprite_colr(req.palette, req.format);
    cmd.srca = req.srca;
    cmd.size = static_cast<uint16_t>(((req.width / 8u) << 8u) | req.height);
    cmd.xa = req.x[0];
    cmd.ya = req.y[0];
    cmd.xb = req.x[1];
    cmd.yb = req.y[1];
    cmd.xc = req.x[2];
    cmd.yc = req.y[2];
    cmd.xd = req.x[3];
    cmd.yd = req.y[3];
    cmd.grda = 0;
    cmd.pad = 0;
    return SAT_OK;
}

namespace {

inline sat_result_t push_polygon_like(uint16_t command_select, const PolygonRequest& req) {
    if (g_cmd_buffer == nullptr) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    SAT_TRY(saturn::core::validate_polygon_effects(req.color, req.flags));
    if (command_slot_unavailable()) {
        return SAT_ERR_CAPACITY;
    }

    Command& cmd = g_cmd_buffer[g_cmd_count++];
    cmd.ctrl = saturn::core::compose_polygon_ctrl(command_select, false);
    cmd.link = 0;
    cmd.pmod = saturn::core::compose_inside_user_clip_pmod(
        saturn::core::compose_polygon_pmod(req.flags), req.user_clip);
    cmd.colr = req.color;
    cmd.srca = 0;
    cmd.size = 0;
    cmd.xa = req.xa;
    cmd.ya = req.ya;
    cmd.xb = req.xb;
    cmd.yb = req.yb;
    cmd.xc = req.xc;
    cmd.yc = req.yc;
    cmd.xd = req.xd;
    cmd.yd = req.yd;
    cmd.grda = 0;
    cmd.pad = 0;
    return SAT_OK;
}

}  // namespace

sat_result_t push_polygon(const PolygonRequest& req) {
    return push_polygon_like(saturn::core::kVdp1CmdPolygon, req);
}

sat_result_t push_polyline(const PolygonRequest& req) {
    return push_polygon_like(saturn::core::kVdp1CmdPolyline, req);
}

sat_result_t push_line(const LineRequest& req) {
    if (g_cmd_buffer == nullptr) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    SAT_TRY(saturn::core::validate_polygon_effects(req.color, req.flags));
    if (command_slot_unavailable()) {
        return SAT_ERR_CAPACITY;
    }

    Command& cmd = g_cmd_buffer[g_cmd_count++];
    cmd.ctrl = saturn::core::compose_polygon_ctrl(saturn::core::kVdp1CmdLine, false);
    cmd.link = 0;
    cmd.pmod = saturn::core::compose_inside_user_clip_pmod(
        saturn::core::compose_polygon_pmod(req.flags), req.user_clip);
    cmd.colr = req.color;
    cmd.srca = 0;
    cmd.size = 0;
    cmd.xa = req.x0;
    cmd.ya = req.y0;
    cmd.xb = req.x1;
    cmd.yb = req.y1;
    cmd.xc = 0;
    cmd.yc = 0;
    cmd.xd = 0;
    cmd.yd = 0;
    cmd.grda = 0;
    cmd.pad = 0;
    return SAT_OK;
}

namespace {

/* Stages the next Gouraud table and returns the CMDGRDA value that points at
 * it. Lines use two corners; C and D stay neutral. */
inline sat_result_t stage_gouraud(const uint16_t* corners, int corner_count, uint16_t* out_grda) {
    if (corners == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (g_gouraud_count >= kGouraudTableCapacity) {
        return SAT_ERR_CAPACITY;
    }
    uint16_t* table = &g_gouraud_words[static_cast<uint32_t>(g_gouraud_count) * 4u];
    for (int i = 0; i < 4; ++i) {
        table[i] = (i < corner_count) ? corners[i] : saturn::core::kGouraudNeutral;
    }
    *out_grda = saturn::core::gouraud_table_grda(kCommandAreaBytes, g_gouraud_count);
    ++g_gouraud_count;
    return SAT_OK;
}

/* Turns the command just pushed into a Gouraud-shaded one, or releases the
 * staged table when the push itself failed. */
inline sat_result_t shade_last_command(sat_result_t pushed, uint16_t grda) {
    if (pushed != SAT_OK) {
        --g_gouraud_count;
        return pushed;
    }
    Command& cmd = g_cmd_buffer[g_cmd_count - 1u];
    cmd.pmod = saturn::core::compose_gouraud_pmod(cmd.pmod);
    cmd.grda = grda;
    return SAT_OK;
}

}  // namespace

sat_result_t push_polygon_gouraud(const PolygonRequest& req, const uint16_t* gouraud) {
    SAT_TRY(saturn::core::validate_polygon_effects(req.color, req.flags));
    uint16_t grda = 0u;
    const sat_result_t st = stage_gouraud(gouraud, 4, &grda);
    if (st != SAT_OK) {
        return st;
    }
    return shade_last_command(push_polygon(req), grda);
}

sat_result_t push_polyline_gouraud(const PolygonRequest& req, const uint16_t* gouraud) {
    SAT_TRY(saturn::core::validate_polygon_effects(req.color, req.flags));
    uint16_t grda = 0u;
    const sat_result_t st = stage_gouraud(gouraud, 4, &grda);
    if (st != SAT_OK) {
        return st;
    }
    return shade_last_command(push_polyline(req), grda);
}

sat_result_t push_line_gouraud(const LineRequest& req, const uint16_t* gouraud) {
    SAT_TRY(saturn::core::validate_polygon_effects(req.color, req.flags));
    uint16_t grda = 0u;
    const sat_result_t st = stage_gouraud(gouraud, 2, &grda);
    if (st != SAT_OK) {
        return st;
    }
    return shade_last_command(push_line(req), grda);
}

void submit() {
    if (g_cmd_buffer == nullptr) {
        return;
    }
    if (g_cmd_count >= g_cmd_capacity) {
        g_cmd_count = static_cast<uint16_t>(g_cmd_capacity - 1u);
    }

    Command& end = g_cmd_buffer[g_cmd_count++];
    end.ctrl = saturn::core::compose_polygon_ctrl(saturn::core::kVdp1CmdNormalSprite, true);
    end.link = 0;
    end.pmod = 0;
    end.colr = 0;
    end.srca = 0;
    end.size = 0;
    end.xa = 0;
    end.ya = 0;
    end.xb = 0;
    end.yb = 0;
    end.xc = 0;
    end.yc = 0;
    end.xd = 0;
    end.yd = 0;
    end.grda = 0;
    end.pad = 0;

    copy_words_to_vram(g_cmd_buffer, g_cmd_count);

    const uint32_t gouraud_word_base = kCommandAreaBytes / 2u;
    const uint32_t gouraud_words = static_cast<uint32_t>(g_gouraud_count) * 4u;
    for (uint32_t i = 0; i < gouraud_words; ++i) {
        VDP1_VRAM_16[gouraud_word_base + i] = g_gouraud_words[i];
    }
    g_frame_submitted=true;
}

sat_result_t upload_palette(const uint16_t* palette_rgb555, uint16_t palette_index) {
    if (palette_rgb555 == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t base = static_cast<uint32_t>(palette_index) * 256u;
    for (uint32_t i = 0; i < 256u; ++i) {
        VDP2_CRAM[base + i] = palette_rgb555[i];
    }
    return SAT_OK;
}

namespace {

sat_result_t validate_indexed8_transfer(
    const uint8_t* pixels, uint16_t width, uint16_t height, uint16_t pitch) {
    if (pixels == nullptr || width == 0u || height == 0u || pitch < width || (width & 7u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (width > 504u || height > 255u) return SAT_ERR_INVALID_ARG;
    return SAT_OK;
}

void write_indexed8_rows(
    uint32_t start, const uint8_t* pixels, uint16_t width, uint16_t height, uint16_t pitch) {
    const uint32_t words_per_row = width / 2u;
    const uint32_t first_word = start / 2u;
    for (uint16_t y = 0u; y < height; ++y) {
        const uint8_t* row = pixels + static_cast<uint32_t>(y) * pitch;
        const uint32_t dst = first_word + static_cast<uint32_t>(y) * words_per_row;
        for (uint32_t x = 0u; x < words_per_row; ++x) {
            const uint16_t hi = row[x * 2u];
            const uint16_t lo = row[x * 2u + 1u];
            VDP1_VRAM_16[dst + x] = static_cast<uint16_t>((hi << 8u) | lo);
        }
    }
}

}  // namespace

sat_result_t upload_texture_indexed8_pitched(
    const uint8_t* pixels, uint16_t width, uint16_t height, uint16_t pitch, uint16_t* out_srca) {
    if (out_srca == nullptr) return SAT_ERR_INVALID_ARG;
    const sat_result_t st = validate_indexed8_transfer(pixels, width, height, pitch);
    if (st != SAT_OK) return st;
    const uint32_t size = static_cast<uint32_t>(width) * static_cast<uint32_t>(height);
    g_texture_cursor = (g_texture_cursor + 7u) & ~7u;
    if (g_texture_cursor + size > kVramSize) return SAT_ERR_CAPACITY;
    const uint32_t start = g_texture_cursor;
    write_indexed8_rows(start, pixels, width, height, pitch);
    *out_srca = static_cast<uint16_t>(start >> 3u);
    g_texture_cursor += size;
    return SAT_OK;
}

sat_result_t upload_texture_indexed8(
    const uint8_t* pixels, uint16_t width, uint16_t height, uint16_t* out_srca) {
    return upload_texture_indexed8_pitched(pixels, width, height, width, out_srca);
}

sat_result_t upload_lut(const uint16_t* lut_rgb555, uint16_t* out_colr) {
    if (lut_rgb555 == nullptr || out_colr == nullptr) return SAT_ERR_INVALID_ARG;
    constexpr uint32_t kLutBytes = 32u;
    g_texture_cursor = (g_texture_cursor + (kLutBytes - 1u)) & ~(kLutBytes - 1u);
    if (g_texture_cursor + kLutBytes > kVramSize) return SAT_ERR_CAPACITY;
    const uint32_t start = g_texture_cursor;
    for (uint32_t i = 0u; i < 16u; ++i) {
        VDP1_VRAM_16[start / 2u + i] = lut_rgb555[i];
    }
    *out_colr = static_cast<uint16_t>(start >> 3u);
    g_texture_cursor += kLutBytes;
    return SAT_OK;
}

sat_result_t upload_texture_lut4(
    const uint8_t* pixels, uint16_t width, uint16_t height, uint16_t* out_srca) {
    if (out_srca == nullptr) return SAT_ERR_INVALID_ARG;
    /* Same geometry rules as INDEXED8, at half a byte per texel. */
    const sat_result_t st = validate_indexed8_transfer(pixels, width, height, width);
    if (st != SAT_OK) return st;
    const uint32_t size = static_cast<uint32_t>(width) * height / 2u;
    g_texture_cursor = (g_texture_cursor + 7u) & ~7u;
    if (g_texture_cursor + size > kVramSize) return SAT_ERR_CAPACITY;
    const uint32_t start = g_texture_cursor;
    write_indexed8_rows(start, pixels, static_cast<uint16_t>(width / 2u), height,
                        static_cast<uint16_t>(width / 2u));
    *out_srca = static_cast<uint16_t>(start >> 3u);
    g_texture_cursor += size;
    return SAT_OK;
}

sat_result_t update_texture_indexed8_pitched(
    uint16_t srca, const uint8_t* pixels, uint16_t width, uint16_t height, uint16_t pitch) {
    const sat_result_t st = validate_indexed8_transfer(pixels, width, height, pitch);
    if (st != SAT_OK) return st;
    const uint32_t start = static_cast<uint32_t>(srca) << 3u;
    const uint32_t size = static_cast<uint32_t>(width) * static_cast<uint32_t>(height);
    if (start < kTextureBase || start + size > g_texture_cursor || start + size > kVramSize) {
        return SAT_ERR_INVALID_ARG;
    }
    write_indexed8_rows(start, pixels, width, height, pitch);
    return SAT_OK;
}

/* INDEX8 patterns are byte-addressed but the VDP1 transfer uses 16-bit
 * writes. Round odd X bounds outward, retaining the adjacent source byte.
 * Validate the complete resident texture range before any VRAM write. */
sat_result_t update_texture_indexed8_rect(
    uint16_t srca, const uint8_t* pixels,
    uint16_t texture_width, uint16_t texture_height, uint16_t pitch,
    uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    const sat_result_t st = validate_indexed8_transfer(
        pixels, texture_width, texture_height, pitch);
    if (st != SAT_OK) return st;
    if (width == 0u || height == 0u ||
        static_cast<uint32_t>(x) + width > texture_width ||
        static_cast<uint32_t>(y) + height > texture_height)
        return SAT_ERR_INVALID_ARG;
    const uint32_t start = static_cast<uint32_t>(srca) << 3u;
    const uint32_t size =
        static_cast<uint32_t>(texture_width) * texture_height;
    if (start < kTextureBase || start + size > g_texture_cursor ||
        start + size > kVramSize) return SAT_ERR_INVALID_ARG;

    const uint16_t first_x = static_cast<uint16_t>(x & ~1u);
    const uint16_t end_x = static_cast<uint16_t>(
        (static_cast<uint32_t>(x) + width + 1u) & ~1u);
    const uint32_t first_word = start / 2u;
    for (uint32_t row = y; row < static_cast<uint32_t>(y) + height; ++row) {
        const uint8_t* source = pixels + row * pitch;
        const uint32_t dst =
            first_word + (row * texture_width + first_x) / 2u;
        for (uint32_t col = first_x; col < end_x; col += 2u)
            VDP1_VRAM_16[dst + (col - first_x) / 2u] =
                static_cast<uint16_t>(
                    (static_cast<uint16_t>(source[col]) << 8u) |
                     static_cast<uint16_t>(source[col + 1u]));
    }
    return SAT_OK;
}

#if SAT_PROFILE_METRICS
extern "C" uint32_t sat_vdp1_test_scene_command_hash(void) {
    return g_test_scene_command_hash;
}

extern "C" uint32_t sat_vdp1_test_scene_command_count(void) {
    return g_test_scene_command_count;
}

extern "C" uint32_t sat_vdp1_test_scene_command_capacity(void) {
    return g_cmd_capacity;
}
#endif

}  // namespace saturn::hal::vdp1
