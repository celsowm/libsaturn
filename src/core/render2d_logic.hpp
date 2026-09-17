#ifndef SATURN_CORE_RENDER2D_LOGIC_HPP
#define SATURN_CORE_RENDER2D_LOGIC_HPP

#include <limits.h>
#include <stdint.h>

#include "saturn/render2d.h"
#include "src/core/math3d_logic.hpp"

namespace saturn::core {

struct Render2DDestination {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    bool scaled;
};

struct Render2DQuad {
    int16_t x[4];
    int16_t y[4];
};

inline bool render2d_neutral_tint(sat_color_t tint) {
    return tint.r == 255u && tint.g == 255u && tint.b == 255u && tint.a == 255u;
}

inline sat_result_t render2d_color_to_direct_rgb555(sat_color_t color, uint16_t* out_color) {
    if (out_color == nullptr) return SAT_ERR_INVALID_ARG;
    if (color.a != 255u) return SAT_ERR_UNSUPPORTED;
    const uint16_t r = static_cast<uint16_t>((static_cast<uint32_t>(color.r) * 31u + 127u) / 255u);
    const uint16_t g = static_cast<uint16_t>((static_cast<uint32_t>(color.g) * 31u + 127u) / 255u);
    const uint16_t b = static_cast<uint16_t>((static_cast<uint32_t>(color.b) * 31u + 127u) / 255u);
    *out_color = SAT_RGB555(r, g, b);
    return SAT_OK;
}

inline sat_result_t validate_render2d_params(const sat_draw_params_t* params) {
    if (params == nullptr) return SAT_OK;
    if (params->flip > static_cast<uint8_t>(SAT_FLIP_X | SAT_FLIP_Y)) return SAT_ERR_INVALID_ARG;
    if (params->blend_mode > SAT_BLEND_SUBTRACT) return SAT_ERR_INVALID_ARG;
    if (params->flags != 0u || params->reserved != 0u) return SAT_ERR_INVALID_ARG;
    if (params->blend_mode != SAT_BLEND_NONE || !render2d_neutral_tint(params->tint)) {
        return SAT_ERR_UNSUPPORTED;
    }
    return SAT_OK;
}

inline sat_result_t resolve_render2d_destination(
    const sat_rect_t* dst,
    uint16_t screen_width,
    uint16_t screen_height,
    uint16_t source_width,
    uint16_t source_height,
    Render2DDestination* out
) {
    if (dst == nullptr || out == nullptr || dst->width == 0u || dst->height == 0u ||
        source_width == 0u || source_height == 0u || screen_width == 0u || screen_height == 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    const int32_t x0 = static_cast<int32_t>(dst->x) - static_cast<int32_t>(screen_width / 2u);
    const int32_t y0 = static_cast<int32_t>(dst->y) - static_cast<int32_t>(screen_height / 2u);
    const int32_t x1 = x0 + static_cast<int32_t>(dst->width) - 1;
    const int32_t y1 = y0 + static_cast<int32_t>(dst->height) - 1;
    const int32_t normal_right = x0 + static_cast<int32_t>(dst->width);
    const int32_t normal_bottom = y0 + static_cast<int32_t>(dst->height);

    if (x0 < INT16_MIN || x0 > INT16_MAX || y0 < INT16_MIN || y0 > INT16_MAX ||
        x1 < INT16_MIN || x1 > INT16_MAX || y1 < INT16_MIN || y1 > INT16_MAX ||
        normal_right < INT16_MIN || normal_right > INT16_MAX ||
        normal_bottom < INT16_MIN || normal_bottom > INT16_MAX) {
        return SAT_ERR_INVALID_ARG;
    }

    out->x0 = static_cast<int16_t>(x0);
    out->y0 = static_cast<int16_t>(y0);
    out->x1 = static_cast<int16_t>(x1);
    out->y1 = static_cast<int16_t>(y1);
    out->scaled = dst->width != source_width || dst->height != source_height;
    return SAT_OK;
}

inline sat_result_t resolve_render2d_quad(
    const sat_rect_t* dst,
    uint16_t screen_width,
    uint16_t screen_height,
    const sat_draw_params_t& params,
    Render2DQuad* out
) {
    if (dst == nullptr || out == nullptr || dst->width == 0u || dst->height == 0u ||
        screen_width == 0u || screen_height == 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    const int32_t cx = params.center.x;
    const int32_t cy = params.center.y;
    const int32_t right = static_cast<int32_t>(dst->width) - 1 - cx;
    const int32_t bottom = static_cast<int32_t>(dst->height) - 1 - cy;
    const int32_t local_x[4] = {-cx, right, right, -cx};
    const int32_t local_y[4] = {-cy, -cy, bottom, bottom};

    const sat_fx16_t sine = math3d::sin_deg_fx(params.rotation);
    const sat_fx16_t cosine = math3d::cos_deg_fx(params.rotation);
    const int32_t pivot_x = static_cast<int32_t>(dst->x) + cx - static_cast<int32_t>(screen_width / 2u);
    const int32_t pivot_y = static_cast<int32_t>(dst->y) + cy - static_cast<int32_t>(screen_height / 2u);

    int32_t geometric_x[4]{};
    int32_t geometric_y[4]{};
    for (uint16_t i = 0u; i < 4u; ++i) {
        const int64_t rotated_x =
            static_cast<int64_t>(local_x[i]) * cosine - static_cast<int64_t>(local_y[i]) * sine;
        const int64_t rotated_y =
            static_cast<int64_t>(local_x[i]) * sine + static_cast<int64_t>(local_y[i]) * cosine;
        const int64_t x = static_cast<int64_t>(pivot_x) + (rotated_x >> 16);
        const int64_t y = static_cast<int64_t>(pivot_y) + (rotated_y >> 16);
        if (x < INT16_MIN || x > INT16_MAX || y < INT16_MIN || y > INT16_MAX) {
            return SAT_ERR_INVALID_ARG;
        }
        geometric_x[i] = static_cast<int32_t>(x);
        geometric_y[i] = static_cast<int32_t>(y);
    }

    uint8_t map[4] = {0u, 1u, 2u, 3u};
    if ((params.flip & SAT_FLIP_X) != 0u) {
        map[0] = 1u; map[1] = 0u; map[2] = 3u; map[3] = 2u;
    }
    if ((params.flip & SAT_FLIP_Y) != 0u) {
        for (uint16_t i = 0u; i < 4u; ++i) map[i] = static_cast<uint8_t>(3u - map[i]);
    }

    for (uint16_t i = 0u; i < 4u; ++i) {
        out->x[i] = static_cast<int16_t>(geometric_x[map[i]]);
        out->y[i] = static_cast<int16_t>(geometric_y[map[i]]);
    }
    return SAT_OK;
}

inline sat_result_t apply_render2d_camera(
    Render2DQuad* quad,
    uint16_t screen_width,
    uint16_t screen_height,
    const sat_camera2d_t& camera
) {
    if (quad == nullptr || screen_width == 0u || screen_height == 0u || camera.zoom <= 0) {
        return SAT_ERR_INVALID_ARG;
    }
    if (camera.offset_x == 0 && camera.offset_y == 0 &&
        camera.target_x == 0 && camera.target_y == 0 &&
        camera.rotation == 0 && camera.zoom == SAT_FX16_ONE) {
        return SAT_OK;
    }

    const sat_fx16_t sine = math3d::sin_deg_fx(camera.rotation);
    const sat_fx16_t cosine = math3d::cos_deg_fx(camera.rotation);
    const int64_t half_w = static_cast<int64_t>(screen_width / 2u);
    const int64_t half_h = static_cast<int64_t>(screen_height / 2u);

    for (uint16_t i = 0u; i < 4u; ++i) {
        const int64_t world_x =
            (static_cast<int64_t>(quad->x[i]) + half_w) << 16;
        const int64_t world_y =
            (static_cast<int64_t>(quad->y[i]) + half_h) << 16;
        const int64_t dx = world_x - static_cast<int64_t>(camera.target_x);
        const int64_t dy = world_y - static_cast<int64_t>(camera.target_y);

        const int64_t scaled_x = (dx * static_cast<int64_t>(camera.zoom)) >> 16;
        const int64_t scaled_y = (dy * static_cast<int64_t>(camera.zoom)) >> 16;
        if (scaled_x < INT32_MIN || scaled_x > INT32_MAX ||
            scaled_y < INT32_MIN || scaled_y > INT32_MAX) {
            return SAT_ERR_INVALID_ARG;
        }

        const int64_t rotated_x =
            (scaled_x * static_cast<int64_t>(cosine) -
             scaled_y * static_cast<int64_t>(sine)) >> 16;
        const int64_t rotated_y =
            (scaled_x * static_cast<int64_t>(sine) +
             scaled_y * static_cast<int64_t>(cosine)) >> 16;
        const int64_t screen_x_fx = static_cast<int64_t>(camera.offset_x) + rotated_x;
        const int64_t screen_y_fx = static_cast<int64_t>(camera.offset_y) + rotated_y;
        const int64_t native_x = (screen_x_fx >> 16) - half_w;
        const int64_t native_y = (screen_y_fx >> 16) - half_h;

        if (native_x < INT16_MIN || native_x > INT16_MAX ||
            native_y < INT16_MIN || native_y > INT16_MAX) {
            return SAT_ERR_INVALID_ARG;
        }
        quad->x[i] = static_cast<int16_t>(native_x);
        quad->y[i] = static_cast<int16_t>(native_y);
    }
    return SAT_OK;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_RENDER2D_LOGIC_HPP */
