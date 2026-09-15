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

/* ------------------------------------------------------------------ */
/* Gouraud shading                                                     */
/* ------------------------------------------------------------------ */
/* The VDP1 interpolates a per-corner RGB correction across a part and adds it
 * to the part's color (VDP1 manual 5.3). A table entry holds three 5-bit
 * values where 10h means "no change", 00h subtracts 16 levels and 1Fh adds
 * 15; results clamp to 0..31. It applies to RGB-coded parts only: polygons,
 * polylines and lines with an RGB color (SAT_RGB555) on the 16bpp frame
 * buffer. A color-bank textured sprite is not RGB-coded, and the 8bpp
 * high-resolution frame buffer allows no color calculation at all.
 *
 * The library manages the table area: every *_gouraud call stages one table
 * for the frame, sat_end_frame copies it to VRAM with the command list, and
 * the command is pointed at it. Returns SAT_ERR_CAPACITY past 2048 tables in
 * one frame. */
#define SAT_GOURAUD_NEUTRAL ((uint16_t)0x4210u)

/* One table entry from per-channel corrections, each clamped to -16..+15. */
static inline uint16_t sat_gouraud_rgb(int dr, int dg, int db) {
    int r = dr + 16;
    int g = dg + 16;
    int b = db + 16;
    r = (r < 0) ? 0 : ((r > 31) ? 31 : r);
    g = (g < 0) ? 0 : ((g > 31) ? 31 : g);
    b = (b < 0) ? 0 : ((b > 31) ? 31 : b);
    return (uint16_t)((b << 10) | (g << 5) | r);
}

/* The same correction on every channel: brightness without a hue shift,
 * what the manual calls white Gouraud. */
static inline uint16_t sat_gouraud_grey(int d) {
    return sat_gouraud_rgb(d, d, d);
}

/* gouraud[0..3] correct corners A..D. */
sat_result_t sat_draw_polygon_gouraud(const sat_polygon_cmd_t* cmd, const uint16_t gouraud[4]);
sat_result_t sat_draw_polyline_gouraud(const sat_polygon_cmd_t* cmd, const uint16_t gouraud[4]);
/* gouraud[0] corrects the start point, gouraud[1] the end point. */
sat_result_t sat_draw_line_gouraud(const sat_line_cmd_t* cmd, const uint16_t gouraud[2]);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP1_H */
