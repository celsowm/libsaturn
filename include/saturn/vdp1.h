#ifndef SATURN_VDP1_H
#define SATURN_VDP1_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Texture                                                             */
/* ------------------------------------------------------------------ */
typedef struct sat_texture {
    uint16_t srca;
    uint16_t width;
    uint16_t height;
    uint16_t palette;
    uint16_t valid;
    uint16_t reserved;
} sat_texture_t;

/* ------------------------------------------------------------------ */
/* Sprite command                                                      */
/* ------------------------------------------------------------------ */
typedef struct sat_sprite_cmd {
    sat_fx16_t x;
    sat_fx16_t y;
    uint16_t width;
    uint16_t height;
    const sat_texture_t* texture;
    uint16_t palette_override;
    uint16_t flags;
} sat_sprite_cmd_t;

/* ------------------------------------------------------------------ */
/* Texture upload & sprite rendering                                   */
/* ------------------------------------------------------------------ */
sat_result_t sat_tex_upload_indexed8(
    sat_texture_t* out_texture,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    const uint16_t* palette_rgb555,
    uint16_t palette_index
);

/* Draws sprite with native VDP1 coordinates.
 * (0,0) = screen center. Coordinates in fixed-point 16.16.
 */
sat_result_t sat_draw_sprite(const sat_sprite_cmd_t* cmd);

/* Draws sprite with screen coordinates (high-level).
 * (0,0) = top-left corner of the screen.
 * Automatically converts to internal VDP1 coordinates.
 */
sat_result_t sat_draw_sprite_screen(
    const sat_texture_t* texture,
    int16_t screen_x,
    int16_t screen_y,
    uint16_t width,
    uint16_t height,
    uint16_t palette_override
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP1_H */
