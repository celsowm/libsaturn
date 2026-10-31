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
    uint8_t sw;          /* sat_vdp2_window_area_t of the sprite window */
} sat_vdp2_screen_window_t;

/* A screen window config for `screen` with every window off and OR logic.
 * Start from it and set the fields you use, so fields added later stay off. */
void sat_vdp2_screen_window_default(sat_vdp2_screen_t screen, sat_vdp2_screen_window_t* out_config);

/* Applies windows to a screen. Both windows must have been defined. The
 * sprite window `sw` is made by palette sprites with window dots
 * (sat_vdp2_sprite_set_palette_window); it cannot be applied to the sprite
 * screen itself. Fill the struct with sat_vdp2_screen_window_default(). */
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

/* Add mode (CCCTL CCMD): every colour calculation on the screen, per-screen
 * and palette sprites alike, adds the two images and saturates instead of
 * mixing them by ratio (ratios are then ignored). One setting for the whole
 * picture. Replayed with the layer registers. BUSY while the sprite colour
 * calculation module (vdp2_color_calc.h) is on: it claims the mode per frame. */
sat_result_t sat_vdp2_color_calc_set_add(uint8_t add);

/* Extended colour calculation (CCCTL bit 10, EXCCEN): the second image
 * becomes the pre-mixed pair of the second and third images (and the line
 * colour screen as well when it is inserted - the manual's four-screen
 * cascade, table 12.2), so the top image blends against that result. Requires
 * normal TV mode: refused while hi-res is on (manual p12_12, p12_14). Unlike
 * hardware blur it is legal with line colour insertion. Mutually exclusive
 * with hardware blur: turning one on clears the other (p12_14 ignores EXCCEN
 * while BOKEN is 1). Replayed by sat_vdp2_layers_commit(). */
sat_result_t sat_vdp2_extended_color_calc_set(uint8_t enabled);

/* Hardware horizontal blur (CCCTL bit 15 BOKEN + bits 14-12 BOKN2..0).
 * `screen` selects which layer the blur operates on, per the manual's BOKN
 * table: 0 sprite, 1 RBG0, 2 NBG0/RBG1, 4 NBG1/EXBG, 5 NBG2, 6 NBG3 (3 and
 * 7 are setting-prohibited and refused). Each dot is mixed with its one- and
 * two-dot-left neighbours at 2:1:1 and forced into the second image, so the
 * blurred screen must not contain transparent dots (manual p12_13). Requires
 * normal TV mode and CRAM mode 0; excludes the line colour screen in both
 * directions - refused while line colour insert is on, and a line colour
 * insert is refused while the blur is on (p12_13). Mutually exclusive with
 * extended colour calculation. Replayed by sat_vdp2_layers_commit(). */
sat_result_t sat_vdp2_hardware_blur_set(uint8_t screen, uint8_t enabled);

/* Colour calculation for hi-res sprites (640/704 wide, where the VDP1 writes
 * 8-bit palette codes that the VDP2 reads as sprite type C). The sprite dots
 * are mixed with the image beneath: by `ratio` (0..31, as for the scroll
 * screens), or added when add mode is on (sat_vdp2_color_calc_set_add). Used
 * with the line colour screen under the sprites
 * (sat_vdp2_sprite_set_line_color_insert) it lights VDP1 polygons that cannot
 * be Gouraud shaded in hi-res; a colour calculation window limits it to an
 * area.
 *   SAT_VDP2_HIRES_SPRITE_CC_ALL: every sprite dot.
 *   SAT_VDP2_HIRES_SPRITE_CC_HIGH_CODES: dots with codes 128-255 only. Code
 *     bit 7 selects the priority register, so those codes keep the sprite
 *     priority p and codes 0-127 get p - 1 (p + 1 when p < 2): draw a polygon
 *     with its colours copied to 128 + k to make it take the effect.
 * Hi-res only (SAT_ERR_UNSUPPORTED otherwise), and only in CRAM mode 0, the
 * one mode in which hi-res mixes palette images (sat_vdp2_cram_mode_set; also
 * SAT_ERR_UNSUPPORTED). SAT_ERR_BUSY while the sprite colour calculation
 * module (vdp2_color_calc.h) is on. Replayed by sat_vdp2_layers_commit(). */
typedef enum sat_vdp2_hires_sprite_cc {
    SAT_VDP2_HIRES_SPRITE_CC_OFF = 0,
    SAT_VDP2_HIRES_SPRITE_CC_ALL = 1,
    SAT_VDP2_HIRES_SPRITE_CC_HIGH_CODES = 2
} sat_vdp2_hires_sprite_cc_t;

sat_result_t sat_vdp2_hires_sprite_color_calc_set(sat_vdp2_hires_sprite_cc_t mode, uint8_t ratio);

/* Ratio register value whose image-below weight is nearest to
 * `below_32nds` (1..32). */
uint8_t sat_vdp2_color_calc_ratio_for_below(uint8_t below_32nds);

/* CRAM bank of a palette screen (NBG0-NBG3, RBG0 or the sprites; CRAOFA and
 * CRAOFB): its colour numbers are offset by bank * 256 colours in CRAM modes 0
 * and 1 (bank 0-7; mode 0 has 1024 colours, so banks 4-7 wrap to 0-3). One
 * palette can then serve several copies (a faded or tinted set for one layer)
 * without touching the tiles. Recorded and written by the next
 * sat_vdp2_layers_commit(), like the layer registers; the hi-res sprite bank
 * (sat_vdp2_sprite_palette_bank_set) is the same setting as SAT_VDP2_SCREEN_SPRITE. */
sat_result_t sat_vdp2_screen_set_cram_bank(sat_vdp2_screen_t screen, uint8_t bank);

/* CRAM mode (RAMCTL CRMD). sat_init leaves mode 1: 2048 colours, CRAM banks
 * 0-7. Mode 0 holds 1024 colours (banks 4-7 wrap to 0-3) but is the only mode
 * in which the hi-res screens (640/704 wide) can mix a palette-format image by
 * colour calculation: a palette sprite or scroll screen, or the line colour
 * screen (VDP2 manual, table 12.1); in mode 1 the hardware skips that mixing.
 * Set the mode before uploading palettes. Kept by every later commit. Modes
 * other than 0 and 1 are SAT_ERR_UNSUPPORTED. */
sat_result_t sat_vdp2_cram_mode_set(uint8_t mode);
uint8_t sat_vdp2_cram_mode(void);

/* Writes the recorded CRAM banks to the registers now. An interrupt handler
 * that switches banks during the picture (a palette swap at a line, written
 * to CRAOFA/CRAOFB directly) calls it at VBlank to put the top-of-picture
 * banks back. No state is changed, so it is safe in a handler. */
void sat_vdp2_cram_banks_apply(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_COMPOSE_H */
