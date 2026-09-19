#ifndef SATURN_RENDER2D_H
#define SATURN_RENDER2D_H

#include <stdint.h>

#include "saturn/color.h"
#include "saturn/core.h"
#include "saturn/geometry2d.h"
#include "saturn/texture.h"
#include "saturn/vdp1.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sat_flip {
    SAT_FLIP_NONE = 0,
    SAT_FLIP_X = 1,
    SAT_FLIP_Y = 2
} sat_flip_t;

typedef enum sat_blend_mode {
    SAT_BLEND_NONE = 0,
    SAT_BLEND_ALPHA = 1,
    SAT_BLEND_ADD = 2,
    SAT_BLEND_SUBTRACT = 3
} sat_blend_mode_t;

typedef struct sat_camera2d {
    /* Screen-space point where target lands, in 16.16 pixels. */
    sat_fx16_t offset_x;
    sat_fx16_t offset_y;
    /* World-space camera target, in 16.16 pixels. */
    sat_fx16_t target_x;
    sat_fx16_t target_y;
    /* Clockwise-positive screen-space rotation in 16.16 degrees. */
    sat_fx16_t rotation;
    /* Positive 16.16 scale; SAT_FX16_ONE is identity. */
    sat_fx16_t zoom;
} sat_camera2d_t;

typedef struct sat_draw_params {
    /* Clockwise-positive screen-space rotation in 16.16 degrees. */
    sat_fx16_t rotation;
    /* Rotation center in destination-local pixels, relative to dst.x/dst.y. */
    sat_point_t center;
    sat_color_t tint;
    uint16_t blend_mode;
    uint16_t flags;
    /* Bitwise SAT_FLIP_X | SAT_FLIP_Y. */
    uint8_t flip;
    uint8_t reserved;
} sat_draw_params_t;

static inline sat_camera2d_t sat_camera2d_default(void) {
    sat_camera2d_t camera;
    camera.offset_x = 0;
    camera.offset_y = 0;
    camera.target_x = 0;
    camera.target_y = 0;
    camera.rotation = 0;
    camera.zoom = SAT_FX16_ONE;
    return camera;
}

static inline sat_draw_params_t sat_draw_params_default(void) {
    sat_draw_params_t params;
    params.rotation = 0;
    params.center.x = 0;
    params.center.y = 0;
    params.tint.r = 255u;
    params.tint.g = 255u;
    params.tint.b = 255u;
    params.tint.a = 255u;
    params.blend_mode = SAT_BLEND_NONE;
    params.flags = 0u;
    params.flip = SAT_FLIP_NONE;
    params.reserved = 0u;
    return params;
}

/* Fixed-capacity render-state stack. Push saves the complete current 2D
 * state; pop restores it. Overflow returns SAT_ERR_CAPACITY and underflow
 * returns SAT_ERR_INVALID_ARG. */
sat_result_t sat_render2d_reset(void);
sat_result_t sat_render2d_push(void);
sat_result_t sat_render2d_pop(void);
sat_result_t sat_render2d_set_camera(const sat_camera2d_t* camera);
sat_result_t sat_render2d_get_camera(sat_camera2d_t* out_camera);
/* Absolute screen-space scissor; Camera2D does not transform it. NULL disables. */
sat_result_t sat_render2d_set_clip(const sat_rect_t* clip);
uint16_t sat_render2d_stack_capacity(void);
uint16_t sat_render2d_stack_depth(void);

/* Shape alpha is discrete on this VDP1 RGB framebuffer: 255 = opaque,
 * 128 = native half-transparency against an *already drawn RGB VDP1 pixel*,
 * 0 = no draw. All other alpha values return SAT_ERR_UNSUPPORTED.
 * On a transparent VDP1 pixel (e.g. just the VDP2 background), hardware
 * half-transparency REPLACES the color instead of blending with VDP2. */
sat_result_t sat_fill_rect(const sat_rect_t* rect, sat_color_t color);
sat_result_t sat_draw_rect(const sat_rect_t* rect, sat_color_t color);
sat_result_t sat_draw_line(sat_point_t start, sat_point_t end, sat_color_t color);

/* Draws a logical texture in top-left world/screen coordinates.
 *
 * src == NULL selects the complete texture. A non-NULL source rectangle is
 * resolved through the runtime's prepared-region cache; PERSISTENT_SOURCE and
 * DYNAMIC textures may materialize a missing region lazily, while UPLOAD_ONLY
 * textures return SAT_ERR_UNSUPPORTED for partial regions.
 *
 * dst is required and its width/height must be non-zero. Scaling, X/Y flip,
 * rotation around params.center, and the current Camera2D are supported.
 * params == NULL is equivalent to sat_draw_params_default(). The current
 * Neutral RGB tint and SAT_BLEND_NONE draw normally. SAT_BLEND_ALPHA
 * interprets tint.a as 0 (skip), 255 (normal), or intermediate alpha via a
 * preconfigured VDP2 sprite color-calculation ratio slot. Call
 * sat_vdp2_sprite_color_calc_configure_alpha() first, and keep the VDP2
 * background at lower priority than both sprite selectors. This mixes
 * sprites with a VDP2 background, NOT overlapping VDP1 sprites.
 * A nearest hardware ratio >2 steps away returns SAT_ERR_UNSUPPORTED.
 * SAT_SPRITE_FLAG_MESH is also supported (checkerboard, not color blending).
 * ADD, SUBTRACT and non-neutral RGB tint remain unsupported. */
sat_result_t sat_draw_texture(
    sat_texture_t texture,
    const sat_rect_t* src,
    const sat_rect_t* dst,
    const sat_draw_params_t* params
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_RENDER2D_H */
