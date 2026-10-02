#include "saturn/render2d.h"
#include "saturn/sprite_clip.h"

/* The texture-facing part of the clip player: region preparation and drawing. */

static_assert(SAT_CLIP_FLIP_X == SAT_FLIP_X && SAT_CLIP_FLIP_Y == SAT_FLIP_Y, "clip flips must match render2d");

extern "C" sat_result_t sat_clip_set_prepare_regions(const sat_clip_set_t* set, sat_texture_t texture) {
    if (!set || (set->clip_count && !set->clips)) return SAT_ERR_INVALID_ARG;
    for (uint32_t i = 0; i < set->clip_count; ++i) {
        const sat_clip_t& c = set->clips[i];
        for (uint32_t f = 0; f < c.frame_count; ++f) {
            const sat_result_t r = sat_texture_prepare_region(texture, &c.frames[f].source);
            if (r != SAT_OK) return r;
        }
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_clip_player_draw(const sat_clip_player_t* player, sat_texture_t texture, int16_t x, int16_t y,
    uint8_t flip, const sat_draw_params_t* params) {
    const sat_clip_frame_t* frame = sat_clip_player_frame(player);
    if (!frame) return SAT_ERR_INVALID_ARG;
    sat_rect_t dst;
    const sat_result_t r = sat_clip_frame_dest(frame, x, y, flip, &dst);
    if (r != SAT_OK) return r;
    sat_draw_params_t p = params ? *params : sat_draw_params_default();
    p.flip = flip & (SAT_CLIP_FLIP_X | SAT_CLIP_FLIP_Y);
    return sat_draw_texture(texture, &frame->source, &dst, &p);
}
