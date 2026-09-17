#include "saturn/render2d.h"

#include "src/core/render2d_logic.hpp"
#include "src/core/runtime_state.hpp"
#include "src/core/texture_runtime.hpp"
#include "src/hal/vdp1.hpp"

namespace {

using saturn::core::TextureRegionRecord;
using saturn::core::TextureSlot;

bool is_full_source_rect(const sat_rect_t& src, const TextureSlot& slot) {
    return src.x == 0 && src.y == 0 &&
           src.width == slot.native.width && src.height == slot.native.height;
}

sat_result_t resolve_draw_source(
    sat_texture_t texture,
    TextureSlot& slot,
    const sat_rect_t* src,
    const sat_vdp1_texture_t** out_native
) {
    using namespace saturn::core;
    if (out_native == nullptr) return SAT_ERR_INVALID_ARG;
    if (src == nullptr || is_full_source_rect(*src, slot)) {
        *out_native = &slot.native;
        return SAT_OK;
    }

    TextureRegionRecord* record = texture_find_region(g_texture_registry, texture, *src);
    if (record == nullptr) {
        const sat_result_t st = sat_texture_prepare_region(texture, src);
        if (st != SAT_OK) return st;
        record = texture_find_region(g_texture_registry, texture, *src);
        if (record == nullptr) return SAT_ERR_INVALID_ARG;
    }
    if (record->native.valid == 0u) return SAT_ERR_INVALID_ARG;
    *out_native = &record->native;
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_draw_texture(
    sat_texture_t texture,
    const sat_rect_t* src,
    const sat_rect_t* dst,
    const sat_draw_params_t* params
) {
    using namespace saturn::core;

    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    st = validate_render2d_params(params);
    if (st != SAT_OK) return st;
    if (dst == nullptr) return SAT_ERR_INVALID_ARG;

    TextureSlot* slot = texture_resolve(g_texture_registry, texture);
    if (slot == nullptr || slot->native.valid == 0u) return SAT_ERR_INVALID_ARG;

    const sat_vdp1_texture_t* native = nullptr;
    st = resolve_draw_source(texture, *slot, src, &native);
    if (st != SAT_OK) return st;

    Render2DDestination resolved{};
    st = resolve_render2d_destination(
        dst,
        g_state.config.width,
        g_state.config.height,
        native->width,
        native->height,
        &resolved);
    if (st != SAT_OK) return st;

    if (!resolved.scaled) {
        saturn::hal::vdp1::SpriteRequest request{};
        request.x = resolved.x0;
        request.y = resolved.y0;
        request.width = native->width;
        request.height = native->height;
        request.srca = native->srca;
        request.palette = native->palette;
        request.flags = 0u;
        return saturn::hal::vdp1::push_sprite(request);
    }

    saturn::hal::vdp1::ScaledSpriteRequest request{};
    request.x0 = resolved.x0;
    request.y0 = resolved.y0;
    request.x1 = resolved.x1;
    request.y1 = resolved.y1;
    request.width = native->width;
    request.height = native->height;
    request.srca = native->srca;
    request.palette = native->palette;
    request.flags = 0u;
    return saturn::hal::vdp1::push_scaled_sprite(request);
}
