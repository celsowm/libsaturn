#include "saturn/render2d.h"

#include "src/graphics/2d/palette/tint.hpp"
#include "src/graphics/2d/rendering/logic.hpp"
#include "src/graphics/vdp1/color_calc_logic.hpp"
#include "saturn/vdp2_color_calc.h"
#include "src/graphics/2d/rendering/runtime.hpp"
#include "src/core/runtime/state.hpp"
#include "src/graphics/2d/textures/runtime.hpp"
#include "src/hal/vdp1/vdp1.hpp"

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

sat_result_t resolve_shape_quad(const sat_rect_t* rect, saturn::core::Render2DQuad* out_quad) {
    using namespace saturn::core;
    if (rect == nullptr || out_quad == nullptr) return SAT_ERR_INVALID_ARG;
    const sat_draw_params_t params = sat_draw_params_default();
    sat_result_t st = resolve_render2d_quad(
        rect,
        g_state.config.width,
        g_state.config.height,
        params,
        out_quad);
    if (st != SAT_OK) return st;
    return apply_render2d_camera(
        out_quad,
        g_state.config.width,
        g_state.config.height,
        g_render2d_runtime.current.camera);
}

}  // namespace

extern "C" sat_result_t sat_render2d_reset(void) {
    using namespace saturn::core;
    const sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    render2d_runtime_reset(g_render2d_runtime);
    return SAT_OK;
}

extern "C" sat_result_t sat_render2d_push(void) {
    using namespace saturn::core;
    const sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    return render2d_runtime_push(g_render2d_runtime);
}

extern "C" sat_result_t sat_render2d_pop(void) {
    using namespace saturn::core;
    const sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    return render2d_runtime_pop(g_render2d_runtime);
}

extern "C" sat_result_t sat_render2d_set_camera(const sat_camera2d_t* camera) {
    using namespace saturn::core;
    const sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (camera == nullptr) return SAT_ERR_INVALID_ARG;
    return render2d_runtime_set_camera(g_render2d_runtime, *camera);
}

extern "C" sat_result_t sat_render2d_get_camera(sat_camera2d_t* out_camera) {
    using namespace saturn::core;
    const sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (out_camera == nullptr) return SAT_ERR_INVALID_ARG;
    *out_camera = g_render2d_runtime.current.camera;
    return SAT_OK;
}

extern "C" sat_result_t sat_render2d_set_clip(const sat_rect_t* clip) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (clip == nullptr) {
        g_render2d_runtime.current.clip = {0, 0, 0u, 0u};
        g_render2d_runtime.current.clip_enabled = 0u;
        g_render2d_runtime.clip_dirty = 0u;
        return SAT_OK;
    }
    Render2DClip resolved{};
    st = resolve_render2d_clip(clip, g_state.config.width, g_state.config.height, &resolved);
    if (st != SAT_OK) return st;
    g_render2d_runtime.current.clip = *clip;
    g_render2d_runtime.current.clip_enabled = 1u;
    g_render2d_runtime.clip_dirty = 1u;
    return SAT_OK;
}

extern "C" uint16_t sat_render2d_stack_capacity(void) {
    return saturn::core::kRender2DStackCapacity;
}

extern "C" uint16_t sat_render2d_stack_depth(void) {
    return saturn::core::g_render2d_runtime.depth;
}

extern "C" sat_result_t sat_render2d_release_tints(void) {
    using namespace saturn::core;
    SAT_TRY(require_initialized());
    tint_release_all(g_tint_cache, g_palette_registry);
    return SAT_OK;
}

extern "C" sat_result_t sat_fill_rect(const sat_rect_t* rect, sat_color_t color) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    st = render2d_ensure_clip(g_state.config.width, g_state.config.height);
    if (st != SAT_OK) return st;

    uint16_t effect_flags = 0u;
    bool skip = false;
    st = render2d_shape_effect(color, &effect_flags, &skip);
    if (st != SAT_OK) return st;
    if (rect == nullptr) return SAT_ERR_INVALID_ARG;
    if (skip) return SAT_OK;
    const sat_color_t opaque_color = {color.r, color.g, color.b, 255u};
    uint16_t direct_color = 0u;
    st = render2d_color_to_direct_rgb555(opaque_color, &direct_color);
    if (st != SAT_OK) return st;

    Render2DQuad quad{};
    st = resolve_shape_quad(rect, &quad);
    if (st != SAT_OK) return st;

    saturn::hal::vdp1::PolygonRequest request{};
    request.xa = quad.x[0];
    request.ya = quad.y[0];
    request.xb = quad.x[1];
    request.yb = quad.y[1];
    request.xc = quad.x[2];
    request.yc = quad.y[2];
    request.xd = quad.x[3];
    request.yd = quad.y[3];
    request.color = direct_color;
    request.flags = effect_flags;
    request.user_clip = g_render2d_runtime.current.clip_enabled != 0u;
    return saturn::hal::vdp1::push_polygon(request);
}

extern "C" sat_result_t sat_draw_rect(const sat_rect_t* rect, sat_color_t color) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    st = render2d_ensure_clip(g_state.config.width, g_state.config.height);
    if (st != SAT_OK) return st;

    uint16_t effect_flags = 0u;
    bool skip = false;
    st = render2d_shape_effect(color, &effect_flags, &skip);
    if (st != SAT_OK) return st;
    if (rect == nullptr) return SAT_ERR_INVALID_ARG;
    if (skip) return SAT_OK;
    const sat_color_t opaque_color = {color.r, color.g, color.b, 255u};
    uint16_t direct_color = 0u;
    st = render2d_color_to_direct_rgb555(opaque_color, &direct_color);
    if (st != SAT_OK) return st;

    Render2DQuad quad{};
    st = resolve_shape_quad(rect, &quad);
    if (st != SAT_OK) return st;

    saturn::hal::vdp1::PolygonRequest request{};
    request.xa = quad.x[0];
    request.ya = quad.y[0];
    request.xb = quad.x[1];
    request.yb = quad.y[1];
    request.xc = quad.x[2];
    request.yc = quad.y[2];
    request.xd = quad.x[3];
    request.yd = quad.y[3];
    request.color = direct_color;
    request.flags = effect_flags;
    request.user_clip = g_render2d_runtime.current.clip_enabled != 0u;
    return saturn::hal::vdp1::push_polyline(request);
}

extern "C" sat_result_t sat_draw_line(
    sat_point_t start,
    sat_point_t end,
    sat_color_t color
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    st = render2d_ensure_clip(g_state.config.width, g_state.config.height);
    if (st != SAT_OK) return st;

    uint16_t effect_flags = 0u;
    bool skip = false;
    st = render2d_shape_effect(color, &effect_flags, &skip);
    if (st != SAT_OK) return st;
    if (skip) return SAT_OK;
    const sat_color_t opaque_color = {color.r, color.g, color.b, 255u};
    uint16_t direct_color = 0u;
    st = render2d_color_to_direct_rgb555(opaque_color, &direct_color);
    if (st != SAT_OK) return st;

    Render2DLine line{};
    st = resolve_render2d_line(
        start,
        end,
        g_state.config.width,
        g_state.config.height,
        g_render2d_runtime.current.camera,
        &line);
    if (st != SAT_OK) return st;

    saturn::hal::vdp1::LineRequest request{};
    request.x0 = line.x0;
    request.y0 = line.y0;
    request.x1 = line.x1;
    request.y1 = line.y1;
    request.color = direct_color;
    request.flags = effect_flags;
    request.user_clip = g_render2d_runtime.current.clip_enabled != 0u;
    return saturn::hal::vdp1::push_line(request);
}

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

    st = render2d_ensure_clip(g_state.config.width, g_state.config.height);
    if (st != SAT_OK) return st;

    const sat_draw_params_t effective = params != nullptr ? *params : sat_draw_params_default();
    uint16_t flags = effective.flags;
    if (effective.tint.a == 0u && effective.blend_mode != SAT_BLEND_NONE) return SAT_OK;

    /* RGB tint: draw through a palette variant multiplied by the tint. For
     * ADD the alpha scales the added colour too, which is exactly
     * dst + src * a. */
    uint16_t bank = native->palette;
    uint8_t tint_r = effective.tint.r;
    uint8_t tint_g = effective.tint.g;
    uint8_t tint_b = effective.tint.b;
    if (effective.blend_mode == SAT_BLEND_ADD && effective.tint.a != 255u) {
        const uint8_t a = effective.tint.a;
        tint_r = static_cast<uint8_t>((static_cast<uint32_t>(tint_r) * a + 127u) / 255u);
        tint_g = static_cast<uint8_t>((static_cast<uint32_t>(tint_g) * a + 127u) / 255u);
        tint_b = static_cast<uint8_t>((static_cast<uint32_t>(tint_b) * a + 127u) / 255u);
    }
    if (tint_r != 255u || tint_g != 255u || tint_b != 255u) {
        /* Hi-res sprites take their bank from CRAOFB for the whole screen. */
        if (g_state.config.width >= 640u || native->format != SAT_VDP1_TEXTURE_INDEXED8) {
            return SAT_ERR_UNSUPPORTED;
        }
        static uint16_t s_tint_scratch[kCramBankEntries];
        SAT_TRY(tint_acquire(
            g_tint_cache, g_palette_registry, native->palette, tint_r, tint_g, tint_b,
            s_tint_scratch,
            [](const uint16_t* palette, uint16_t variant) {
                return saturn::hal::vdp1::upload_palette(palette, variant);
            },
            &bank));
    }

    uint16_t palette = bank;
    if (effective.blend_mode == SAT_BLEND_ALPHA) {
        if (effective.tint.a != 255u) {
            uint8_t slot_id = 0u;
            SAT_TRY(sat_vdp2_sprite_color_calc_alpha_slot(effective.tint.a, &slot_id, nullptr));
            SAT_TRY(sat_vdp2_sprite_color_calc_claim_mode(SAT_VDP2_COLOR_CALC_RATIO));
            SAT_TRY(vdp1_color_calc::encode_palette_selector(bank, slot_id, &palette));
        }
    } else if (effective.blend_mode == SAT_BLEND_ADD) {
        /* Add mode ignores the ratio registers, so any slot selects the
         * colour-calculated priority; slot 0 it is. */
        SAT_TRY(sat_vdp2_sprite_color_calc_claim_mode(SAT_VDP2_COLOR_CALC_ADD));
        SAT_TRY(vdp1_color_calc::encode_palette_selector(bank, 0u, &palette));
    } else if (effective.blend_mode == SAT_BLEND_SUBTRACT) {
        /* VDP1 shadow: the sprite's opaque texels halve the RGB pixels
         * already drawn under them. */
        flags = static_cast<uint16_t>(flags | SAT_SPRITE_FLAG_SHADOW);
    }
    const sat_camera2d_t& camera = g_render2d_runtime.current.camera;
    if (effective.rotation != 0 || effective.flip != SAT_FLIP_NONE ||
        !render2d_camera_is_identity(camera)) {
        Render2DQuad quad{};
        st = resolve_render2d_quad(
            dst,
            g_state.config.width,
            g_state.config.height,
            effective,
            &quad);
        if (st != SAT_OK) return st;
        st = apply_render2d_camera(
            &quad,
            g_state.config.width,
            g_state.config.height,
            camera);
        if (st != SAT_OK) return st;

        saturn::hal::vdp1::DistortedSpriteRequest request{};
        for (uint16_t i = 0u; i < 4u; ++i) {
            request.x[i] = quad.x[i];
            request.y[i] = quad.y[i];
        }
        request.width = native->width;
        request.height = native->height;
        request.srca = native->srca;
        request.palette = palette;
        request.flags = flags;
        request.user_clip = g_render2d_runtime.current.clip_enabled != 0u;
        return saturn::hal::vdp1::push_distorted_sprite(request);
    }

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
        request.palette = palette;
        request.flags = flags;
        request.user_clip = g_render2d_runtime.current.clip_enabled != 0u;
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
    request.palette = palette;
    request.flags = flags;
    request.user_clip = g_render2d_runtime.current.clip_enabled != 0u;
    return saturn::hal::vdp1::push_scaled_sprite(request);
}
