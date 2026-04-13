#include "saturn/vdp1.h"

#include "src/core/internal.hpp"
#include "src/core/logic.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/vdp1.hpp"

extern "C" sat_result_t sat_tex_upload_indexed8(
    sat_texture_t* out_texture,
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
    return saturn::hal::vdp1::push_sprite(req);
}

extern "C" sat_result_t sat_draw_sprite_screen(
    const sat_texture_t* texture,
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

    // Convert screen coordinates (0,0 = top-left corner)
    // to local VDP1 coordinates (0,0 = screen center).
    // VDP1 draws from the top-left corner of the sprite (xa, ya).
    // To center sprite at (screen_x, screen_y):
    //   vdp1_x = (screen_x - TV_WIDTH/2) - (sprite_width/2)
    //   vdp1_y = (screen_y - TV_HEIGHT/2) - (sprite_height/2)
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
