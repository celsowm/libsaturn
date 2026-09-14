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

/* Sprite flags                                                        */
#define SAT_SPRITE_FLAG_OPAQUE 0x0001u

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
/* Scaled sprite command (command select 0001B)                        */
/* ------------------------------------------------------------------ */
/* Draws the texture into the axis-aligned rectangle whose top-left is
 * vertex A (x0,y0) and bottom-right is vertex C (x1,y1), in native VDP1
 * coordinates (0,0 = screen center). This is the two-coordinate form of the
 * rectangular sprite command (zoom point = 0). */
typedef struct sat_scaled_sprite_cmd {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    const sat_texture_t* texture;
    uint16_t palette_override;
    uint16_t flags;
} sat_scaled_sprite_cmd_t;

/* ------------------------------------------------------------------ */
/* Distorted sprite command (command select 0010B)                     */
/* ------------------------------------------------------------------ */
/* Draws the texture into an arbitrary quad, enabling rotation and true
 * perspective faces. Texture corners map A=top-left, B=top-right,
 * C=bottom-right, D=bottom-left. Native VDP1 coordinates (0,0 = center). */
typedef struct sat_distorted_sprite_cmd {
    int16_t x[4];
    int16_t y[4];
    const sat_texture_t* texture;
    uint16_t palette_override;
    uint16_t flags;
} sat_distorted_sprite_cmd_t;

/* ------------------------------------------------------------------ */
/* Polygon / polyline / line commands                                 */
/* ------------------------------------------------------------------ */
typedef struct sat_polygon_cmd {
    int16_t x[4];
    int16_t y[4];
    uint16_t color;      /* RGB555 (set bit 15) or color bank code */
    uint16_t flags;
} sat_polygon_cmd_t;

typedef struct sat_line_cmd {
    int16_t x0, y0;
    int16_t x1, y1;
    uint16_t color;      /* RGB555 (set bit 15) or color bank code */
    uint16_t flags;
} sat_line_cmd_t;

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

/* Uploads one 256-entry palette without touching texture VRAM. Use this once
 * for a model whose many baked face textures share a palette, then upload
 * each texture with sat_tex_upload_indexed8_pixels. */
sat_result_t sat_palette_upload_indexed8(
    const uint16_t* palette_rgb555,
    uint16_t palette_index
);

/* Uploads indexed8 texels that reference an already-uploaded palette. Unlike
 * sat_tex_upload_indexed8 this performs no palette upload, so N faces sharing
 * one palette cost one CRAM write instead of N. Width/height validation and
 * VRAM allocation are identical to the combined path; exhaustion returns
 * SAT_ERR_CAPACITY. */
sat_result_t sat_tex_upload_indexed8_pixels(
    sat_texture_t* out_texture,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
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

/* Draws a scaled sprite with native VDP1 coordinates (0,0 = screen center).
 * The texture's own width/height define CMDSIZE; (x0,y0)-(x1,y1) define the
 * destination rectangle, so non-uniform scaling is supported. */
sat_result_t sat_draw_sprite_scaled(const sat_scaled_sprite_cmd_t* cmd);

/* Draws a scaled sprite centred on a SCREEN coordinate (0,0 = top-left).
 *
 * VDP1 texture width must be a multiple of 8, which is often not the size a
 * sprite wants to be on screen -- an actor on an 8-pixel tile grid wants to be
 * around 12 pixels, and the nearest legal texture is 16. Scaling at draw time
 * is how those two meet, and doing it through screen coordinates keeps the
 * call site from hand-rolling the centre-origin conversion. */
sat_result_t sat_draw_sprite_scaled_screen(
    const sat_texture_t* texture,
    int16_t screen_x,
    int16_t screen_y,
    uint16_t draw_width,
    uint16_t draw_height,
    uint16_t palette_override
);

/* Draws a distorted (arbitrary-quad) sprite with native VDP1 coordinates.
 * Region order is A(top-left), B(top-right), C(bottom-right), D(bottom-left). */
sat_result_t sat_draw_sprite_distorted(const sat_distorted_sprite_cmd_t* cmd);

/* Draws a filled polygon (4 vertices) with native VDP1 coordinates.
 * (0,0) = screen center.
 */
sat_result_t sat_draw_polygon(const sat_polygon_cmd_t* cmd);

/* Draws a filled, axis-aligned rectangle in SCREEN coordinates
 * ((0,0) = top-left), converting to native VDP1 coordinates internally.
 * `color` must be RGB-coded (see SAT_RGB555). A zero width or height draws
 * nothing and still returns SAT_OK.
 */
sat_result_t sat_draw_rect_screen(
    int16_t x,
    int16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color
);

/* Draws an unfilled polyline (4 vertices, outline only) with native VDP1 coordinates.
 * (0,0) = screen center.
 */
sat_result_t sat_draw_polyline(const sat_polygon_cmd_t* cmd);

/* Draws a single line (2 endpoints) with native VDP1 coordinates.
 * (0,0) = screen center.
 */
sat_result_t sat_draw_line(const sat_line_cmd_t* cmd);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP1_H */
