#include "saturn/vdp1.h"

#include "src/core/internal.hpp"
#include "src/core/logic.hpp"
#include "src/core/palette_registry.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/vdp1.hpp"

extern "C" sat_result_t sat_tex_upload_indexed8(
    sat_vdp1_texture_t* out_texture,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    const uint16_t* palette_rgb555,
    uint16_t palette_index
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (out_texture == nullptr || pixels == nullptr || palette_rgb555 == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    st = validate_palette_bank(palette_index);
    if (st != SAT_OK) {
        return st;
    }
    st = validate_indexed8_texture_dims(width, height);
    if (st != SAT_OK) {
        return st;
    }
    st = palette_claim_external(g_palette_registry, static_cast<uint16_t>(palette_index * 256u), 256u);
    if (st != SAT_OK) {
        return st;
    }

    st = saturn::hal::vdp1::upload_palette(palette_rgb555, palette_index);
    if (st != SAT_OK) {
        return st;
    }

    uint16_t srca = 0;
    st = saturn::hal::vdp1::upload_texture_indexed8(pixels, width, height, &srca);
    if (st != SAT_OK) {
        return st;
    }

    out_texture->srca = srca;
    out_texture->width = width;
    out_texture->height = height;
    out_texture->palette = palette_index;
    out_texture->valid = 1;
    out_texture->reserved = 0;
    return SAT_OK;
}

extern "C" sat_result_t sat_palette_upload_indexed8(
    const uint16_t* palette_rgb555,
    uint16_t palette_index
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (palette_rgb555 == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    st = validate_palette_bank(palette_index);
    if (st != SAT_OK) {
        return st;
    }
    st = palette_claim_external(g_palette_registry, static_cast<uint16_t>(palette_index * 256u), 256u);
    if (st != SAT_OK) {
        return st;
    }
    return saturn::hal::vdp1::upload_palette(palette_rgb555, palette_index);
}

extern "C" sat_result_t sat_tex_upload_indexed8_pixels(
    sat_vdp1_texture_t* out_texture,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t palette_index
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (out_texture == nullptr || pixels == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    st = validate_palette_bank(palette_index);
    if (st != SAT_OK) {
        return st;
    }
    st = validate_indexed8_texture_dims(width, height);
    if (st != SAT_OK) {
        return st;
    }
    st = palette_claim_external(g_palette_registry, static_cast<uint16_t>(palette_index * 256u), 256u);
    if (st != SAT_OK) {
        return st;
    }

    uint16_t srca = 0;
    st = saturn::hal::vdp1::upload_texture_indexed8(pixels, width, height, &srca);
    if (st != SAT_OK) {
        return st;
    }

    out_texture->srca = srca;
    out_texture->width = width;
    out_texture->height = height;
    out_texture->palette = palette_index;
    out_texture->valid = 1;
    out_texture->reserved = 0;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp1_reserve_overlay_commands(uint16_t count) {
    SAT_TRY(saturn::core::require_initialized());
    return saturn::hal::vdp1::reserve_overlay_commands(count);
}

extern "C" sat_result_t sat_vdp1_overlay_begin(void) {
    SAT_TRY(saturn::core::require_initialized());
    return saturn::hal::vdp1::begin_overlay_pass();
}

extern "C" sat_result_t sat_draw_sprite(const sat_sprite_cmd_t* cmd) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    ResolvedSprite resolved = {};
    st = resolve_sprite_cmd(cmd, &resolved);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp1::SpriteRequest req = {};
    req.x = resolved.x;
    req.y = resolved.y;
    req.width = resolved.width;
    req.height = resolved.height;
    req.srca = resolved.srca;
    req.palette = resolved.palette;
    req.flags = resolved.flags;
    return saturn::hal::vdp1::push_sprite(req);
}

extern "C" sat_result_t sat_draw_sprite_screen(
    const sat_vdp1_texture_t* texture,
    int16_t screen_x,
    int16_t screen_y,
    uint16_t width,
    uint16_t height,
    uint16_t palette_override
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (texture == nullptr || texture->valid == 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    const uint16_t vdp1_w = (width != 0u) ? width : texture->width;
    const uint16_t vdp1_h = (height != 0u) ? height : texture->height;
    const int16_t vdp1_x = screen_x - 160 - static_cast<int16_t>(vdp1_w / 2);
    const int16_t vdp1_y = screen_y - 112 - static_cast<int16_t>(vdp1_h / 2);

    sat_sprite_cmd_t cmd = {
        (sat_fx16_t)((int32_t)vdp1_x << 16),
        (sat_fx16_t)((int32_t)vdp1_y << 16),
        vdp1_w,
        vdp1_h,
        texture,
        palette_override,
        0
    };
    return sat_draw_sprite(&cmd);
}

extern "C" sat_result_t sat_draw_sprite_scaled(const sat_scaled_sprite_cmd_t* cmd) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    ResolvedScaledSprite resolved = {};
    st = resolve_scaled_sprite_cmd(cmd, &resolved);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp1::ScaledSpriteRequest req = {};
    req.x0 = resolved.x0;
    req.y0 = resolved.y0;
    req.x1 = resolved.x1;
    req.y1 = resolved.y1;
    req.width = resolved.width;
    req.height = resolved.height;
    req.srca = resolved.srca;
    req.palette = resolved.palette;
    req.flags = resolved.flags;
    return saturn::hal::vdp1::push_scaled_sprite(req);
}

extern "C" sat_result_t sat_draw_sprite_scaled_screen(
    const sat_vdp1_texture_t* texture,
    int16_t screen_x,
    int16_t screen_y,
    uint16_t draw_width,
    uint16_t draw_height,
    uint16_t palette_override
) {
    using namespace saturn::core;
    const sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (texture == nullptr || texture->valid == 0u || draw_width == 0u || draw_height == 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    const sat_video_config_t& cfg = g_state.config;
    const int32_t left = static_cast<int32_t>(screen_x) - static_cast<int32_t>(draw_width / 2u);
    const int32_t top = static_cast<int32_t>(screen_y) - static_cast<int32_t>(draw_height / 2u);

    sat_scaled_sprite_cmd_t cmd = {};
    cmd.x0 = saturn::internal::screen_to_native(static_cast<int>(left), cfg.width);
    cmd.y0 = saturn::internal::screen_to_native(static_cast<int>(top), cfg.height);
    cmd.x1 = static_cast<int16_t>(cmd.x0 + static_cast<int16_t>(draw_width) - 1);
    cmd.y1 = static_cast<int16_t>(cmd.y0 + static_cast<int16_t>(draw_height) - 1);
    cmd.texture = texture;
    cmd.palette_override = palette_override;
    cmd.flags = 0;
    return sat_draw_sprite_scaled(&cmd);
}

extern "C" sat_result_t sat_draw_sprite_distorted(const sat_distorted_sprite_cmd_t* cmd) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    ResolvedDistortedSprite resolved = {};
    st = resolve_distorted_sprite_cmd(cmd, &resolved);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp1::DistortedSpriteRequest req = {};
    for (int i = 0; i < 4; ++i) {
        req.x[i] = resolved.x[i];
        req.y[i] = resolved.y[i];
    }
    req.width = resolved.width;
    req.height = resolved.height;
    req.srca = resolved.srca;
    req.palette = resolved.palette;
    req.flags = resolved.flags;
    return saturn::hal::vdp1::push_distorted_sprite(req);
}

extern "C" sat_result_t sat_vdp1_draw_polygon(const sat_polygon_cmd_t* cmd) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (cmd == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    saturn::hal::vdp1::PolygonRequest req = {};
    req.xa = cmd->x[0];
    req.ya = cmd->y[0];
    req.xb = cmd->x[1];
    req.yb = cmd->y[1];
    req.xc = cmd->x[2];
    req.yc = cmd->y[2];
    req.xd = cmd->x[3];
    req.yd = cmd->y[3];
    req.color = cmd->color;
    req.flags = cmd->flags;
    return saturn::hal::vdp1::push_polygon(req);
}

extern "C" sat_result_t sat_vdp1_draw_polyline(const sat_polygon_cmd_t* cmd) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (cmd == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    saturn::hal::vdp1::PolygonRequest req = {};
    req.xa = cmd->x[0];
    req.ya = cmd->y[0];
    req.xb = cmd->x[1];
    req.yb = cmd->y[1];
    req.xc = cmd->x[2];
    req.yc = cmd->y[2];
    req.xd = cmd->x[3];
    req.yd = cmd->y[3];
    req.color = cmd->color;
    req.flags = cmd->flags;
    return saturn::hal::vdp1::push_polyline(req);
}

extern "C" sat_result_t sat_vdp1_draw_line(const sat_line_cmd_t* cmd) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (cmd == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    saturn::hal::vdp1::LineRequest req = {};
    req.x0 = cmd->x0;
    req.y0 = cmd->y0;
    req.x1 = cmd->x1;
    req.y1 = cmd->y1;
    req.color = cmd->color;
    req.flags = cmd->flags;
    return saturn::hal::vdp1::push_line(req);
}

namespace {

saturn::hal::vdp1::PolygonRequest polygon_request(const sat_polygon_cmd_t* cmd) {
    saturn::hal::vdp1::PolygonRequest req = {};
    req.xa = cmd->x[0];
    req.ya = cmd->y[0];
    req.xb = cmd->x[1];
    req.yb = cmd->y[1];
    req.xc = cmd->x[2];
    req.yc = cmd->y[2];
    req.xd = cmd->x[3];
    req.yd = cmd->y[3];
    req.color = cmd->color;
    req.flags = cmd->flags;
    return req;
}

}  // namespace

extern "C" sat_result_t sat_vdp1_draw_polygon_gouraud(
    const sat_polygon_cmd_t* cmd,
    const uint16_t gouraud[4]
) {
    sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (cmd == nullptr || gouraud == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return saturn::hal::vdp1::push_polygon_gouraud(polygon_request(cmd), gouraud);
}

extern "C" sat_result_t sat_vdp1_draw_polyline_gouraud(
    const sat_polygon_cmd_t* cmd,
    const uint16_t gouraud[4]
) {
    sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (cmd == nullptr || gouraud == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return saturn::hal::vdp1::push_polyline_gouraud(polygon_request(cmd), gouraud);
}

extern "C" sat_result_t sat_vdp1_draw_line_gouraud(const sat_line_cmd_t* cmd, const uint16_t gouraud[2]) {
    sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (cmd == nullptr || gouraud == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    saturn::hal::vdp1::LineRequest req = {};
    req.x0 = cmd->x0;
    req.y0 = cmd->y0;
    req.x1 = cmd->x1;
    req.y1 = cmd->y1;
    req.color = cmd->color;
    req.flags = cmd->flags;
    return saturn::hal::vdp1::push_line_gouraud(req, gouraud);
}
