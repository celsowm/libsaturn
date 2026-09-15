#ifndef SATURN_CORE_ANIM3D_LOGIC_HPP
#define SATURN_CORE_ANIM3D_LOGIC_HPP

/* Pure, host-testable baked-animation math: validation, fixed-point time
 * accumulation (NTSC/PAL agnostic) and int16 pose decoding.
 *
 * No hardware access, so tests/host/test_anim3d_logic.cpp links this
 * directly. The public C API in include/saturn/anim3d.h is a thin wrapper.
 */

#include <stdint.h>

#include "saturn/anim3d.h"
#include "saturn/core.h"
#include "src/core/math3d_logic.hpp"

namespace saturn::core::anim3d {

inline const sat_model_animation_asset* clip_at(
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    if (asset == nullptr || asset->animations == nullptr ||
        clip >= asset->animation_count) {
        return nullptr;
    }
    return &asset->animations[clip];
}

inline sat_result_t validate_clip(const sat_animated_model_asset* asset, uint16_t clip) {
    if (asset == nullptr || asset->model == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_model_animation_asset* anim = clip_at(asset, clip);
    if (anim == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (anim->frame_count == 0u || anim->vertex_count == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (anim->positions == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (anim->sample_rate_num == 0u || anim->sample_rate_den == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (anim->vertex_count != asset->model->vertex_count) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

inline sat_result_t validate(const sat_animated_model_asset* asset) {
    if (asset == nullptr || asset->model == nullptr ||
        asset->animations == nullptr || asset->animation_count == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    for (uint16_t c = 0; c < asset->animation_count; ++c) {
        const sat_result_t st = validate_clip(asset, c);
        if (st != SAT_OK) {
            return st;
        }
        /* Every shade index must land inside the palette. This walk is
         * O(frames x faces), so it lives here -- called once at load --
         * rather than in the per-frame paths, which keep O(1) clip checks. */
        const sat_model_animation_asset* anim = &asset->animations[c];
        if (anim->face_shades != nullptr) {
            const sat_model_asset* model = asset->model;
            if (model->shade_palette_rgb555 == nullptr || model->shade_palette_count == 0u) {
                return SAT_ERR_INVALID_ARG;
            }
            const uint32_t n =
                static_cast<uint32_t>(anim->frame_count) * static_cast<uint32_t>(model->face_count);
            for (uint32_t i = 0; i < n; ++i) {
                if (anim->face_shades[i] >= model->shade_palette_count) {
                    return SAT_ERR_INVALID_ARG;
                }
            }
        }
    }
    return SAT_OK;
}

/* Clip duration in 16.16 seconds: frames / (num/den). */
inline sat_fx16_t clip_duration_fx(const sat_model_animation_asset* anim) {
    const int64_t num = static_cast<int64_t>(anim->frame_count) *
        static_cast<int64_t>(anim->sample_rate_den) * 65536;
    return static_cast<sat_fx16_t>(num / static_cast<int64_t>(anim->sample_rate_num));
}

/* Frame index for a 16.16 time: floor(time * num / (den * 65536)),
 * wrapped for loops and held on the final frame otherwise. */
inline uint16_t frame_for_time(const sat_model_animation_asset* anim, sat_fx16_t time) {
    const int64_t raw =
        (static_cast<int64_t>(time) * static_cast<int64_t>(anim->sample_rate_num)) /
        (static_cast<int64_t>(anim->sample_rate_den) * 65536);
    if (raw < 0) {
        return 0u;
    }
    if (raw >= static_cast<int64_t>(anim->frame_count)) {
        if ((anim->flags & SAT_ANIM_FLAG_LOOP) != 0u) {
            return static_cast<uint16_t>(raw % static_cast<int64_t>(anim->frame_count));
        }
        return static_cast<uint16_t>(anim->frame_count - 1u);
    }
    return static_cast<uint16_t>(raw);
}

inline sat_result_t state_init(
    sat_anim_state_t* state,
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    if (state == nullptr || validate_clip(asset, clip) != SAT_OK) {
        return SAT_ERR_INVALID_ARG;
    }
    state->clip = clip;
    state->frame = 0u;
    state->flags = 0u;
    state->reserved = 0u;
    state->time = 0;
    return SAT_OK;
}

inline sat_result_t advance(
    sat_anim_state_t* state,
    const sat_animated_model_asset* asset,
    sat_fx16_t dt
) {
    if (state == nullptr || validate_clip(asset, state->clip) != SAT_OK) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((state->flags & SAT_ANIM_STATE_PAUSED) != 0u) {
        return SAT_OK;
    }
    const sat_model_animation_asset* anim = clip_at(asset, state->clip);
    state->time += dt;
    if (state->time < 0) {
        state->time = 0;
    }
    const sat_fx16_t dur = clip_duration_fx(anim);
    if ((anim->flags & SAT_ANIM_FLAG_LOOP) != 0u) {
        if (state->time >= dur) {
            state->time %= dur;
        }
    } else if (state->time >= dur) {
        state->time = dur;
    }
    state->frame = frame_for_time(anim, state->time);
    return SAT_OK;
}

/* floor(n / 32767) for 0 <= |n| < 2^30, truncating toward zero, with no
 * divide: m = ceil(2^45 / 32767) = 1073774594 overshoots 2^45 / 32767 by
 * e = 32766 / 32767, and n * e < 2^45 across that whole range, so the
 * reciprocal multiply is exact. */
inline int32_t div32767_small(int32_t n) {
    const uint32_t mag = static_cast<uint32_t>((n < 0) ? -n : n);
    const uint32_t q =
        static_cast<uint32_t>((static_cast<uint64_t>(mag) * 1073774594u) >> 45u);
    return (n < 0) ? -static_cast<int32_t>(q) : static_cast<int32_t>(q);
}

/* One axis of the quantization contract, bias + trunc(scale * q / 32767),
 * split so the per-vertex part never divides.
 *
 * Decoding used to spend three 64-bit divides per vertex per frame -- the
 * single largest cost of an animated model, even on the hardware divider.
 * scale = whole * 32767 + rest is taken once per clip axis instead; whole * q
 * is then exact, rest * q stays under 2^30, and since both parts share the
 * sign of scale their truncations add up to the truncation of the sum. */
struct axis_decoder {
    sat_fx16_t bias;
    int32_t whole;
    int32_t rest;
};

inline axis_decoder make_axis_decoder(sat_fx16_t bias, sat_fx16_t scale) {
    axis_decoder d;
    d.bias = bias;
    d.whole = scale / 32767;
    d.rest = scale - (d.whole * 32767);
    return d;
}

inline sat_fx16_t decode_axis(const axis_decoder& d, int16_t q) {
    return d.bias + static_cast<sat_fx16_t>(d.whole * q) + div32767_small(d.rest * q);
}

inline sat_result_t decode(
    const sat_animated_model_asset* asset,
    const sat_anim_state_t* state,
    sat_vec3_t* out,
    uint16_t cap
) {
    if (asset == nullptr || state == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (validate_clip(asset, state->clip) != SAT_OK) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_model_animation_asset* anim = clip_at(asset, state->clip);
    if (state->frame >= anim->frame_count) {
        return SAT_ERR_INVALID_ARG;
    }
    if (cap < anim->vertex_count) {
        return SAT_ERR_CAPACITY;
    }
    const uint32_t base =
        static_cast<uint32_t>(state->frame) * static_cast<uint32_t>(anim->vertex_count) * 3u;
    const axis_decoder ax = make_axis_decoder(anim->encoding.bias_x, anim->encoding.scale_x);
    const axis_decoder ay = make_axis_decoder(anim->encoding.bias_y, anim->encoding.scale_y);
    const axis_decoder az = make_axis_decoder(anim->encoding.bias_z, anim->encoding.scale_z);
    const int16_t* q = &anim->positions[base];
    for (uint16_t v = 0; v < anim->vertex_count; ++v) {
        out[v].x = decode_axis(ax, q[0]);
        out[v].y = decode_axis(ay, q[1]);
        out[v].z = decode_axis(az, q[2]);
        q += 3;
    }
    return SAT_OK;
}

inline sat_result_t face_colors(
    const sat_animated_model_asset* asset,
    const sat_anim_state_t* state,
    uint16_t* out,
    uint16_t cap
) {
    if (asset == nullptr || state == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (validate_clip(asset, state->clip) != SAT_OK) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_model_animation_asset* anim = clip_at(asset, state->clip);
    const sat_model_asset* model = asset->model;
    if (state->frame >= anim->frame_count) {
        return SAT_ERR_INVALID_ARG;
    }
    if (anim->face_shades == nullptr || model->shade_palette_rgb555 == nullptr ||
        model->shade_palette_count == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (cap < model->face_count) {
        return SAT_ERR_CAPACITY;
    }
    const uint8_t* shade =
        &anim->face_shades[static_cast<uint32_t>(state->frame) * model->face_count];
    const uint16_t* palette = model->shade_palette_rgb555;
    const uint16_t count = model->shade_palette_count;
    for (uint16_t f = 0; f < model->face_count; ++f) {
        /* sat_anim_validate already bounds every index; clamping here costs
         * one compare and keeps an unvalidated asset from reading past it. */
        const uint8_t i = shade[f];
        out[f] = palette[(i < count) ? i : 0u];
    }
    return SAT_OK;
}

}  // namespace saturn::core::anim3d

#endif /* SATURN_CORE_ANIM3D_LOGIC_HPP */
