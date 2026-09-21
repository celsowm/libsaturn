#ifndef SATURN_SPRITE_ANIM_H
#define SATURN_SPRITE_ANIM_H

#include <stdint.h>
#include "saturn/vdp1.h"
#include "saturn/texture.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A bounded direction x frame view over already-uploaded textures.  The
 * descriptor never owns VRAM and never allocates; it only centralizes the
 * selection contract used by game-facing sprite instances. */
typedef struct sat_sprite_anim {
    const sat_vdp1_texture_t* frames;
    uint8_t direction_count;
    uint8_t frame_count;
    uint8_t direction;
    uint8_t frame;
} sat_sprite_anim_t;

sat_result_t sat_sprite_anim_init(sat_sprite_anim_t* anim,
                                  const sat_vdp1_texture_t* frames,
                                  uint8_t direction_count,
                                  uint8_t frame_count);
sat_result_t sat_sprite_anim_set(sat_sprite_anim_t* anim,
                                 uint8_t direction, uint8_t frame);
const sat_vdp1_texture_t* sat_sprite_anim_texture(
    const sat_sprite_anim_t* anim);

typedef struct sat_sprite_region_anim {
    sat_texture_t texture;
    const sat_rect_t* frames;
    uint16_t frame_count;
    uint16_t frame;
} sat_sprite_region_anim_t;

sat_result_t sat_sprite_region_anim_init(
    sat_sprite_region_anim_t* anim, sat_texture_t texture,
    const sat_rect_t* frames, uint16_t frame_count);
sat_result_t sat_sprite_region_anim_set(
    sat_sprite_region_anim_t* anim, uint16_t frame);
sat_result_t sat_sprite_region_anim_source(
    const sat_sprite_region_anim_t* anim, sat_rect_t* out_source);

#ifdef __cplusplus
}
#endif
#endif
