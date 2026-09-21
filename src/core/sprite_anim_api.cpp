#include "saturn/sprite_anim.h"

extern "C" sat_result_t sat_sprite_anim_init(
    sat_sprite_anim_t* anim, const sat_vdp1_texture_t* frames,
    uint8_t direction_count, uint8_t frame_count) {
    if (!anim || !frames || !direction_count || !frame_count)
        return SAT_ERR_INVALID_ARG;
    *anim = {};
    anim->frames = frames;
    anim->direction_count = direction_count;
    anim->frame_count = frame_count;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_anim_set(
    sat_sprite_anim_t* anim, uint8_t direction, uint8_t frame) {
    if (!anim || !anim->frames || direction >= anim->direction_count ||
        frame >= anim->frame_count)
        return SAT_ERR_INVALID_ARG;
    anim->direction = direction;
    anim->frame = frame;
    return SAT_OK;
}

extern "C" const sat_vdp1_texture_t* sat_sprite_anim_texture(
    const sat_sprite_anim_t* anim) {
    if (!anim || !anim->frames || anim->direction >= anim->direction_count ||
        anim->frame >= anim->frame_count)
        return nullptr;
    return &anim->frames[(uint16_t)anim->direction * anim->frame_count + anim->frame];
}

extern "C" sat_result_t sat_sprite_region_anim_init(
    sat_sprite_region_anim_t* anim, sat_texture_t texture,
    const sat_rect_t* frames, uint16_t frame_count) {
    if (!anim || !texture.generation || !frames || !frame_count)
        return SAT_ERR_INVALID_ARG;
    *anim = {};
    anim->texture = texture;
    anim->frames = frames;
    anim->frame_count = frame_count;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_region_anim_set(
    sat_sprite_region_anim_t* anim, uint16_t frame) {
    if (!anim || !anim->frames || frame >= anim->frame_count)
        return SAT_ERR_INVALID_ARG;
    anim->frame = frame;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_region_anim_source(
    const sat_sprite_region_anim_t* anim, sat_rect_t* out_source) {
    if (!anim || !anim->frames || !out_source ||
        anim->frame >= anim->frame_count)
        return SAT_ERR_INVALID_ARG;
    *out_source = anim->frames[anim->frame];
    return SAT_OK;
}
