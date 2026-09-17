#ifndef SATURN_CORE_RENDER2D_LOGIC_HPP
#define SATURN_CORE_RENDER2D_LOGIC_HPP

#include <limits.h>
#include <stdint.h>

#include "saturn/render2d.h"

namespace saturn::core {

struct Render2DDestination {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    bool scaled;
};

inline bool render2d_neutral_tint(sat_color_t tint) {
    return tint.r == 255u && tint.g == 255u && tint.b == 255u && tint.a == 255u;
}

inline sat_result_t validate_render2d_params(const sat_draw_params_t* params) {
    if (params == nullptr) return SAT_OK;
    if (params->flip > static_cast<uint8_t>(SAT_FLIP_X | SAT_FLIP_Y)) return SAT_ERR_INVALID_ARG;
    if (params->blend_mode > SAT_BLEND_SUBTRACT) return SAT_ERR_INVALID_ARG;
    if (params->flags != 0u || params->reserved != 0u) return SAT_ERR_INVALID_ARG;
    if (params->rotation != 0 || params->flip != SAT_FLIP_NONE ||
        params->blend_mode != SAT_BLEND_NONE || !render2d_neutral_tint(params->tint)) {
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
    /* Normal sprites derive B/C/D by adding width/height to A, so validate one
     * pixel beyond the inclusive scaled-sprite endpoint as well. */
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

}  // namespace saturn::core

#endif /* SATURN_CORE_RENDER2D_LOGIC_HPP */
