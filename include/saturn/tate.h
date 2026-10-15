#ifndef SATURN_TATE_H
#define SATURN_TATE_H

#include <stdint.h>
#include "saturn/core.h"
#include "saturn/geometry2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TATE: a vertical (portrait) game on a landscape display turned on its side.
 * The game keeps one logical playfield, upright, e.g. 224 x 448, and this
 * module maps it onto the physical screen:
 *
 *   SAT_TATE_UPRIGHT  no rotation (a portrait monitor, or a small field)
 *   SAT_TATE_ROT_CW   the logical top is the screen's right edge; the image
 *                     is turned a quarter clockwise (monitor turned CCW)
 *   SAT_TATE_ROT_CCW  the logical top is the screen's left edge
 *
 * Coordinates are pixel EDGES: logical x runs 0..logical_w, y 0..logical_h, and
 * pixel (x, y) is the square from (x, y) to (x + 1, y + 1). Mapping edges
 * (rather than pixel indices) makes a rectangle land on exactly the screen
 * pixels that hold its logical pixels, rotated, and two neighbouring
 * rectangles still touch with no gap or overlap. A sprite that is rotated
 * offline (tools/shmup_assets.py pack --orientation) takes its screen anchor
 * from sat_tate_edge applied to the logical anchor, and its pivot from the
 * packer's rotated pivot, which uses the same edge convention.
 *
 * The playfield is centred on the screen and drawn 1:1 when it fits; when it
 * does not it is scaled by scale_num / scale_den (the largest ratio that
 * fits) and the layout reports exact = 0. Screen not covered by the field is
 * returned as up to two side panels for a HUD. The module only does the
 * arithmetic: it touches no hardware, so it is the same on every video mode.
 *
 * Hi-res 640 x 224 with a 224 x 448 playfield turned a quarter: the field is
 * 448 x 224, 1:1, centred, with a 96 pixel panel on each side. Normal 320 x
 * 224: 5/7 scale, a 320 x 160 field, 32 lines above and below. */
#define SAT_TATE_UPRIGHT 0u
#define SAT_TATE_ROT_CW 1u
#define SAT_TATE_ROT_CCW 2u

typedef struct sat_tate_layout {
    uint16_t logical_w;
    uint16_t logical_h;
    uint16_t screen_w;
    uint16_t screen_h;
    uint16_t scale_num;
    uint16_t scale_den;
    uint8_t rotation;
    uint8_t exact;           /* 1: scale is 1:1, mappings are lossless */
    uint8_t panel_count;     /* 0..2 free screen strips beside / above the field */
    uint8_t reserved;
    sat_rect_t field;        /* the playfield on the screen */
    sat_rect_t panel[2];     /* free strips: left/top first, right/bottom second */
} sat_tate_layout_t;

sat_result_t sat_tate_layout_init(sat_tate_layout_t* layout,
                                  uint16_t screen_w, uint16_t screen_h,
                                  uint16_t logical_w, uint16_t logical_h,
                                  uint8_t rotation);

/* Logical edge point -> screen edge point (screen pixel coordinates, origin
 * top-left). Points on the field's edges are accepted; outside is not. */
sat_result_t sat_tate_edge(const sat_tate_layout_t* layout, int32_t lx, int32_t ly,
                           int32_t* out_sx, int32_t* out_sy);

/* Logical rectangle (edges: x, y, width, height) -> the screen rectangle that
 * holds it. Exact when layout->exact. */
sat_result_t sat_tate_rect(const sat_tate_layout_t* layout, const sat_rect_t* logical,
                           sat_rect_t* out_screen);

/* Screen edge point -> logical edge point; the inverse of sat_tate_edge when
 * exact, the nearest below otherwise. */
sat_result_t sat_tate_unmap_edge(const sat_tate_layout_t* layout, int32_t sx, int32_t sy,
                                 int32_t* out_lx, int32_t* out_ly);

/* Logical pixel -> screen pixel that holds it (its top-left in logical terms
 * is not the screen's top-left once rotated; this resolves it). */
sat_result_t sat_tate_pixel(const sat_tate_layout_t* layout, int32_t lx, int32_t ly,
                            int32_t* out_sx, int32_t* out_sy);

/* The rotated size of a w x h logical sprite or box. */
void sat_tate_size(const sat_tate_layout_t* layout, uint16_t w, uint16_t h,
                   uint16_t* out_w, uint16_t* out_h);

/* Hardware TATE: the VDP1 draws the playfield UPRIGHT at the top left of its frame buffer in
 * rotation mode (TVMR TVM = 011b, 512 x 512 x 8 bits, 320/352 x 224 display only) and the VDP2
 * reads the frame buffer back through rotation parameter A, turned and scaled as `layout` says
 * (see examples/tate_fb_rotation). Sprites need no pre-rotation.
 *
 * Fills the 48 big-endian 16-bit words of rotation parameter table A (96 bytes, the VDP2 reads
 * the first 0x58) so that screen pixel (sx, sy) shows the frame buffer pixel
 *   sat_tate_unmap_edge(layout, sx, sy)
 * to 1/1024 pixel: Xst/Yst, DXst/DYst, DX/DY, the matrix as identity and kx = ky = 1. The
 * table must sit in VDP2 VRAM at the word address RBG0's rotation parameter base names. Fails
 * with SAT_ERR_INVALID_ARG when the logical field does not fit a 512 x 512 buffer or a value
 * overflows the table's 13.10 fields. */
#define SAT_TATE_FB_TABLE_WORDS 48u

sat_result_t sat_tate_fb_rotation_table(const sat_tate_layout_t* layout, uint16_t* out_words);

#ifdef __cplusplus
}
#endif
#endif
