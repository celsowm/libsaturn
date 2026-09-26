#ifndef SATURN_VDP2_COMPOSE_H
#define SATURN_VDP2_COMPOSE_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* How the VDP2 composes its screens: windows, mosaic and colour calculation
 * per screen. The registers are write-only, so the library keeps a copy and
 * replays it with sat_vdp2_layers_commit() like the layer registers.
 *
 * Windows. The VDP2 has two windows, W0 and W1. Each is a rectangle or, as a
 * line window, a horizontal extent per line read from a VRAM table (any shape
 * that is one span per line: circles, diamonds, a spotlight). A screen picks
 * which windows apply to it and whether it is shown inside or outside each;
 * with both in use they are combined with OR or AND. Coordinates are TV dots
 * (x 0..319 in the normal modes, y 0..223), start and end both inclusive; a
 * start beyond its end leaves the whole screen outside the window. */

typedef enum sat_vdp2_screen {
    SAT_VDP2_SCREEN_NBG0 = 0,
    SAT_VDP2_SCREEN_NBG1 = 1,
    SAT_VDP2_SCREEN_NBG2 = 2,
    SAT_VDP2_SCREEN_NBG3 = 3,
    SAT_VDP2_SCREEN_RBG0 = 4,
    SAT_VDP2_SCREEN_SPRITE = 5,
    SAT_VDP2_SCREEN_COLOR_CALC = 6   /* windows only: where colour calculation applies */
} sat_vdp2_screen_t;

typedef struct sat_vdp2_window_rect {
    uint16_t x0, y0, x1, y1;
} sat_vdp2_window_rect_t;

typedef struct sat_vdp2_window_span {
    uint16_t x0, x1;   /* first and last dot inside on that line */
} sat_vdp2_window_span_t;

/* Defines window 0 or 1 as a rectangle. */
sat_result_t sat_vdp2_window_set_rect(uint8_t window, const sat_vdp2_window_rect_t* rect);

/* Defines the window as a line window between lines y0 and y1 (inclusive),
 * reading its spans from a table at `table_address` (a byte address in VDP2
 * VRAM, a multiple of 4) that has one span, two words, for every screen line
 * from 0 to y1: line n's span is at table_address + 4 * n. It must not overlap
 * anything else in VRAM. */
sat_result_t sat_vdp2_window_set_line_table(uint8_t window, uint32_t table_address,
                                            uint16_t y0, uint16_t y1);

/* Writes spans into a line window's table starting at screen line
 * `first_line`. */
sat_result_t sat_vdp2_window_line_write(uint8_t window, uint32_t first_line,
                                        const sat_vdp2_window_span_t* spans, uint32_t count);

/* A filled ellipse of radii rx, ry around (cx, cy) as a line window; writes
 * the table (cy + ry + 1 entries from `table_address`) and sets it all up. */
sat_result_t sat_vdp2_window_set_ellipse(uint8_t window, uint32_t table_address,
                                         int32_t cx, int32_t cy, int32_t rx, int32_t ry);

/* Stops using the window; screens that referenced it lose that reference. */
sat_result_t sat_vdp2_window_clear(uint8_t window);

typedef enum sat_vdp2_window_area {
    SAT_VDP2_WINDOW_AREA_OFF = 0,       /* window not used by this screen */
    SAT_VDP2_WINDOW_AREA_INSIDE = 1,    /* shown inside the window */
    SAT_VDP2_WINDOW_AREA_OUTSIDE = 2    /* shown outside the window */
} sat_vdp2_window_area_t;

typedef struct sat_vdp2_screen_window {
    uint8_t screen;      /* sat_vdp2_screen_t */
    uint8_t w0;          /* sat_vdp2_window_area_t */
    uint8_t w1;          /* sat_vdp2_window_area_t */
    uint8_t logic_and;   /* 0: shown where either window says so, 1: where both do */
} sat_vdp2_screen_window_t;

/* Applies windows to a screen. Both windows must have been defined. The
 * sprite window (driven by the sprite data's top bit) is not offered: it
 * needs palette-only sprites, which the library's RGB sprite path does not
 * use. */
sat_result_t sat_vdp2_screen_window_set(const sat_vdp2_screen_window_t* config);

/* Back to no window on that screen. */
sat_result_t sat_vdp2_screen_window_clear(sat_vdp2_screen_t screen);

/* Mosaic: every screen with it enabled shows the top-left dot of each
 * width x height block (1..16). The size is shared by all screens. NBG0 and
 * NBG1 lose vertical cell scroll while mosaic is on: enabling it on such a
 * layer, or configuring cell scroll on a mosaic layer, is refused with
 * SAT_ERR_UNSUPPORTED. RBG0 is mosaicked horizontally only. */
sat_result_t sat_vdp2_mosaic_set_size(uint8_t width, uint8_t height);
sat_result_t sat_vdp2_screen_set_mosaic(sat_vdp2_screen_t screen, uint8_t enabled);

/* Colour calculation for a scroll screen (NBG0-NBG3, RBG0): where it is the
 * top image it is mixed with the image beneath. `ratio` is the hardware value
 * 0..31: 0 keeps 31/32 of this screen and 1/32 of the image below, 31 shows
 * only the image below (see the ratio table in the VDP2 manual, chapter 12).
 * The mixing mode is the one the sprite colour calculation module selects for
 * the frame: add-as-is (SAT_BLEND_ADD) ignores the ratio. Use a colour
 * calculation window to switch the mix off in an area. */
sat_result_t sat_vdp2_screen_color_calc_set(sat_vdp2_screen_t screen, uint8_t enabled, uint8_t ratio);

/* Ratio register value whose image-below weight is nearest to
 * `below_32nds` (1..32). */
uint8_t sat_vdp2_color_calc_ratio_for_below(uint8_t below_32nds);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_COMPOSE_H */
