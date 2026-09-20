#ifndef SATURN_VDP1_H
#define SATURN_VDP1_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Native VDP1 texture                                                */
/* ------------------------------------------------------------------ */
/* Hardware-facing representation. srca is a VDP1 VRAM character address and
 * palette is a CRAM bank. Game-facing/runtime texture APIs must not expose
 * this structure. */
typedef struct sat_vdp1_texture {
    uint16_t srca;
    uint16_t width;
    uint16_t height;
    uint16_t palette;
    uint16_t valid;
    uint16_t reserved;
} sat_vdp1_texture_t;

/* Sprite flags                                                        */
#define SAT_SPRITE_FLAG_OPAQUE 0x0001u
/* CMDPMOD bit 8: checkerboard 50% coverage, not an alpha blend. */
#define SAT_SPRITE_FLAG_MESH 0x0002u
/* CMDPMOD color calculation 011B: (RGB source + RGB framebuffer)/2.
 * The pre-existing framebuffer pixel must have MSB=1. On an empty/VDP2-only
 * background this mode REPLACES the pixel; it does NOT blend with VDP2.
 * Supported on RGB-coded polygons, lines and Gouraud polygons, not INDEX8
 * palette sprites. VDP1 color calculation has substantial rendering cost. */
#define SAT_SPRITE_FLAG_HALF_TRANSPARENT 0x0004u
/* CMDPMOD color calculation 010B: halves RGB source brightness, without
 * mixing with the underlying framebuffer. */
#define SAT_SPRITE_FLAG_HALF_LUMINANCE 0x0008u

/* ------------------------------------------------------------------ */
/* Sprite command                                                      */
/* ------------------------------------------------------------------ */
typedef struct sat_sprite_cmd {
    sat_fx16_t x;
    sat_fx16_t y;
    uint16_t width;
    uint16_t height;
    const sat_vdp1_texture_t* texture;
    uint16_t palette_override;
    uint16_t flags;
} sat_sprite_cmd_t;

typedef struct sat_scaled_sprite_cmd {
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    const sat_vdp1_texture_t* texture;
    uint16_t palette_override;
    uint16_t flags;
} sat_scaled_sprite_cmd_t;

typedef struct sat_distorted_sprite_cmd {
    int16_t x[4];
    int16_t y[4];
    const sat_vdp1_texture_t* texture;
    uint16_t palette_override;
    uint16_t flags;
} sat_distorted_sprite_cmd_t;

typedef struct sat_polygon_cmd {
    int16_t x[4];
    int16_t y[4];
    uint16_t color;
    uint16_t flags;
} sat_polygon_cmd_t;

typedef struct sat_line_cmd {
    int16_t x0, y0;
    int16_t x1, y1;
    uint16_t color;
    uint16_t flags;
} sat_line_cmd_t;

/* Native command APIs intentionally carry the VDP1 prefix. High-level
 * screen/world-space shapes live in render2d.h and use sat_color_t/Camera2D. */

sat_result_t sat_tex_upload_indexed8(
    sat_vdp1_texture_t* out_texture,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    const uint16_t* palette_rgb555,
    uint16_t palette_index
);

sat_result_t sat_palette_upload_indexed8(
    const uint16_t* palette_rgb555,
    uint16_t palette_index
);

sat_result_t sat_tex_upload_indexed8_pixels(
    sat_vdp1_texture_t* out_texture,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t palette_index
);

/* Call immediately after sat_begin_frame to reserve command-list slots for
 * an overlay/HUD drawn AFTER the world. In the world pass the shared VDP1
 * writer enforces this reservation for every sprite/polygon/line command;
 * SAT_ERR_CAPACITY then means world budget exhausted, NOT that HUD was lost.
 * Begin the final overlay pass just before issuing HUD commands; reservations
 * then become drawable slots. The END command is always reserved separately.
 *
 * The overlay pass is irreversible until the next sat_begin_frame. This is a
 * command-list budget, not a GPU raster-time guarantee. Callers must handle a
 * world draw returning SAT_ERR_CAPACITY without aborting the HUD pass. */
sat_result_t sat_vdp1_reserve_overlay_commands(uint16_t count);
sat_result_t sat_vdp1_overlay_begin(void);

/* Command-list occupancy, for debug overlays and capacity tuning.
 *
 * Running out of commands is the one failure a well-behaved caller is
 * expected to swallow -- draw calls return SAT_ERR_CAPACITY and decorations
 * are optional -- which also makes it invisible. A frame that silently stops
 * drawing part of a character looks like a clipping or animation bug, and
 * there is no way to tell from outside without this. `used` counts commands
 * issued this frame, excluding the END terminator; `overlay_reserved` is the
 * quota still withheld from the world pass. */
typedef struct sat_vdp1_command_stats {
    uint16_t used;
    uint16_t capacity;
    uint16_t overlay_reserved;
    uint8_t overlay_pass;
    uint8_t reserved;
} sat_vdp1_command_stats_t;

sat_result_t sat_vdp1_command_stats(sat_vdp1_command_stats_t* out);

sat_result_t sat_draw_sprite(const sat_sprite_cmd_t* cmd);

sat_result_t sat_draw_sprite_screen(
    const sat_vdp1_texture_t* texture,
    int16_t screen_x,
    int16_t screen_y,
    uint16_t width,
    uint16_t height,
    uint16_t palette_override
);

sat_result_t sat_draw_sprite_scaled(const sat_scaled_sprite_cmd_t* cmd);

sat_result_t sat_draw_sprite_scaled_screen(
    const sat_vdp1_texture_t* texture,
    int16_t screen_x,
    int16_t screen_y,
    uint16_t draw_width,
    uint16_t draw_height,
    uint16_t palette_override
);

sat_result_t sat_draw_sprite_distorted(const sat_distorted_sprite_cmd_t* cmd);
sat_result_t sat_vdp1_draw_polygon(const sat_polygon_cmd_t* cmd);

sat_result_t sat_draw_rect_screen(
    int16_t x,
    int16_t y,
    uint16_t width,
    uint16_t height,
    uint16_t color
);

sat_result_t sat_vdp1_draw_polyline(const sat_polygon_cmd_t* cmd);
sat_result_t sat_vdp1_draw_line(const sat_line_cmd_t* cmd);

#define SAT_GOURAUD_NEUTRAL ((uint16_t)0x4210u)

static inline uint16_t sat_gouraud_rgb(int dr, int dg, int db) {
    int r = dr + 16;
    int g = dg + 16;
    int b = db + 16;
    r = (r < 0) ? 0 : ((r > 31) ? 31 : r);
    g = (g < 0) ? 0 : ((g > 31) ? 31 : g);
    b = (b < 0) ? 0 : ((b > 31) ? 31 : b);
    return (uint16_t)((b << 10) | (g << 5) | r);
}

static inline uint16_t sat_gouraud_grey(int d) {
    return sat_gouraud_rgb(d, d, d);
}

sat_result_t sat_vdp1_draw_polygon_gouraud(const sat_polygon_cmd_t* cmd, const uint16_t gouraud[4]);
sat_result_t sat_vdp1_draw_polyline_gouraud(const sat_polygon_cmd_t* cmd, const uint16_t gouraud[4]);
sat_result_t sat_vdp1_draw_line_gouraud(const sat_line_cmd_t* cmd, const uint16_t gouraud[2]);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP1_H */
