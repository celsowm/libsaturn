#include "saturn/anim3d.h"

#include "src/core/anim3d_logic.hpp"

extern "C" sat_result_t sat_anim_validate(const sat_animated_model_asset* asset) {
    return saturn::core::anim3d::validate(asset);
}

extern "C" sat_result_t sat_anim_clip_validate(
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    return saturn::core::anim3d::validate_clip(asset, clip);
}

extern "C" sat_fx16_t sat_anim_clip_duration(
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    const sat_model_animation_asset* anim = saturn::core::anim3d::clip_at(asset, clip);
    if (anim == nullptr || anim->sample_rate_num == 0u || anim->sample_rate_den == 0u ||
        anim->frame_count == 0u) {
        return 0;
    }
    return saturn::core::anim3d::clip_duration_fx(anim);
}

extern "C" sat_result_t sat_anim_state_init(
    sat_anim_state_t* state,
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    return saturn::core::anim3d::state_init(state, asset, clip);
}

extern "C" sat_result_t sat_anim_set_clip(
    sat_anim_state_t* state,
    const sat_animated_model_asset* asset,
    uint16_t clip
) {
    return saturn::core::anim3d::state_init(state, asset, clip);
}

extern "C" sat_result_t sat_anim_reset(sat_anim_state_t* state) {
    if (state == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    state->frame = 0u;
    state->time = 0;
    return SAT_OK;
}

extern "C" void sat_anim_set_paused(sat_anim_state_t* state, int paused) {
    if (state == nullptr) {
        return;
    }
    if (paused != 0) {
        state->flags = static_cast<uint16_t>(state->flags | SAT_ANIM_STATE_PAUSED);
    } else {
        state->flags = static_cast<uint16_t>(
            state->flags & static_cast<uint16_t>(~SAT_ANIM_STATE_PAUSED));
    }
}

extern "C" int sat_anim_is_paused(const sat_anim_state_t* state) {
    if (state == nullptr) {
        return 0;
    }
    return (state->flags & SAT_ANIM_STATE_PAUSED) != 0u ? 1 : 0;
}

extern "C" sat_result_t sat_anim_advance(
    sat_anim_state_t* state,
    const sat_animated_model_asset* asset,
    sat_fx16_t dt
) {
    return saturn::core::anim3d::advance(state, asset, dt);
}

extern "C" sat_result_t sat_anim_decode(
    const sat_animated_model_asset* asset,
    const sat_anim_state_t* state,
    sat_vec3_t* out_vertices,
    uint16_t vertex_cap
) {
    return saturn::core::anim3d::decode(asset, state, out_vertices, vertex_cap);
}
