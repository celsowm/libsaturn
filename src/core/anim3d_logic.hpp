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

/* One quantized axis back to 16.16: bias + scale * q / 32767. */
inline sat_fx16_t decode_axis(sat_fx16_t bias, sat_fx16_t scale, int16_t q) {
    return bias +
        static_cast<sat_fx16_t>(
            (static_cast<int64_t>(scale) * static_cast<int64_t>(q)) / 32767);
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
    for (uint16_t v = 0; v < anim->vertex_count; ++v) {
        const uint32_t slot = base + static_cast<uint32_t>(v) * 3u;
        out[v].x = decode_axis(anim->encoding.bias_x, anim->encoding.scale_x, anim->positions[slot]);
        out[v].y = decode_axis(anim->encoding.bias_y, anim->encoding.scale_y, anim->positions[slot + 1u]);
        out[v].z = decode_axis(anim->encoding.bias_z, anim->encoding.scale_z, anim->positions[slot + 2u]);
    }
    return SAT_OK;
}

}  // namespace saturn::core::anim3d

#endif /* SATURN_CORE_ANIM3D_LOGIC_HPP */
