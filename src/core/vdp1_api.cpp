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

    st = hal::vdp1::upload_palette(palette_rgb555, palette_index);
    if (st != SAT_OK) {
        return st;
    }

    uint16_t srca = 0;
    st = hal::vdp1::upload_texture_indexed8(pixels, width, height, &srca);
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

    hal::vdp1::SpriteRequest req = {};
    req.x = resolved.x;
    req.y = resolved.y;
    req.width = resolved.width;
    req.height = resolved.height;
    req.srca = resolved.srca;
    req.palette = resolved.palette;
    return hal::vdp1::push_sprite(req);
}
