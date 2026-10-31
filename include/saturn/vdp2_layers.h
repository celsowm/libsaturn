#ifndef SATURN_VDP2_LAYERS_H
#define SATURN_VDP2_LAYERS_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/vdp2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The four normal scroll screens, NBG0 to NBG3, managed together.
 *
 * Several layers at once are only possible if the VDP2 can fetch all of their
 * VRAM data within one access cycle (8 timings per VRAM bank, manual chapter
 * 3). This module works that out: sat_vdp2_layer_configure() places every
 * layer's pattern-name, character and vertical-cell-scroll reads into the
 * cycle pattern registers under the manual's rules, or refuses with
 * SAT_ERR_CAPACITY and leaves the previous setup untouched. Where the data
 * lives decides which banks are read, so the addresses in the configuration
 * matter: see the bank map below.
 *
 * This manager owns the VRAM cycle registers, BGON and the NBG registers. The
 * older sat_vdp2_nbg0_* / sat_vdp2_rbg0_* calls own them too and cannot be
 * mixed with it: each side returns SAT_ERR_BUSY while the other is in use. */

typedef enum sat_vdp2_layer {
    SAT_VDP2_NBG0 = 0,
    SAT_VDP2_NBG1 = 1,
    SAT_VDP2_NBG2 = 2,
    SAT_VDP2_NBG3 = 3
} sat_vdp2_layer_t;

/* VRAM banks (sat_vdp2_init divides VRAM-A and VRAM-B in two):
 *   A0 0x00000-0x1FFFF   A1 0x20000-0x3FFFF   B0 0x40000-0x5FFFF   B1 0x60000-0x7FFFF
 * Addresses below are byte offsets into VDP2 VRAM (word offset * 2). */
#define SAT_VDP2_BANK_A0 ((uint8_t)0x01)
#define SAT_VDP2_BANK_A1 ((uint8_t)0x02)
#define SAT_VDP2_BANK_B0 ((uint8_t)0x04)
#define SAT_VDP2_BANK_B1 ((uint8_t)0x08)

#define SAT_VDP2_BITMAP_512X256   0u
#define SAT_VDP2_BITMAP_512X512   1u
#define SAT_VDP2_BITMAP_1024X256  2u
#define SAT_VDP2_BITMAP_1024X512  3u

typedef struct sat_vdp2_layer_config {
    uint8_t layer;               /* sat_vdp2_layer_t */
    uint8_t bitmap;              /* 1: bitmap format (NBG0, NBG1); 0: cell format */
    uint8_t color_mode;          /* sat_vdp2_color_mode_t; NBG2/NBG3: 16 or 256 colours */
    uint8_t char_size;           /* cell: sat_vdp2_char_size_t */
    uint8_t pattern_name_words;  /* cell: 1 or 2 */
    uint8_t plane_pages_x;       /* cell: plane size in 64x64-cell pages, 1 or 2 */
    uint8_t plane_pages_y;       /* cell: 1 or 2 */
    uint8_t bitmap_size;         /* bitmap: SAT_VDP2_BITMAP_* */
    uint8_t priority;            /* 0 (hidden) to 7 */
    uint8_t transparent;         /* 1: dot code 0 is transparent */
    uint8_t palette;             /* bitmap: palette number 0-7; cell 1-word names,
                                    16 colours: palette number bits 6-4 */
    uint8_t char_bank_mask;      /* cell: SAT_VDP2_BANK_* holding the character data */
    /* Cell format: byte addresses of the pattern name tables of planes A..D
     * (the map is 2x2 planes), each a multiple of the plane size (1 page =
     * 0x2000 bytes for 1-word names and 1x1 characters, 0x800 for 2x2, 0x4000
     * and 0x1000 for 2-word names) and agreeing above bit 6 of the map number.
     * Bitmap format: [0] is the bitmap base, a multiple of 0x20000. */
    uint32_t plane_address[4];
    /* Cell, 1-word names: byte address of the 32 KiB window (0x20000 for 2x2
     * characters) that the 10-bit character numbers index; character number
     * n is at base + n * 0x20. */
    uint32_t char_base_address;
    uint8_t vertical_cell_scroll;    /* NBG0/NBG1: read a vertical cell scroll table */
    uint8_t reserved0;
    uint16_t reserved1;
    uint32_t vertical_cell_scroll_address;   /* byte address of that table */
} sat_vdp2_layer_config_t;

/* Fills sensible defaults for a layer: 1-word names, 1x1 characters, 16
 * colours, one page per plane, priority 1, dot 0 transparent. The caller sets
 * the addresses. */
void sat_vdp2_layer_config_default(sat_vdp2_layer_t layer, sat_vdp2_layer_config_t* out_config);

/* Sets a layer up and shows it. SAT_ERR_INVALID_ARG for a malformed
 * configuration (bad alignment, an illegal colour count for the layer ...),
 * SAT_ERR_CAPACITY when the layers together cannot be fetched, SAT_ERR_BUSY
 * while the old NBG0/RBG0 calls are in use. Nothing changes on failure. */
sat_result_t sat_vdp2_layer_configure(const sat_vdp2_layer_config_t* config);

/* Stops managing the layer and hides it; its VRAM reads are freed. */
sat_result_t sat_vdp2_layer_release(sat_vdp2_layer_t layer);

/* Double-buffer publication for a one-plane cell layer: moves all four
 * pattern-name tables to `byte_address` at the next
 * sat_vdp2_layers_commit() VBlank replay. The new ring must already hold the
 * picture for the scroll that commit will apply; both rings are expected to
 * live in the same VRAM bank and map-offset group. No register is touched
 * here, so writing the inactive ring during the picture is tear-free.
 * Supports cell format with all four plane_address entries equal; bitmap or
 * multi-plane maps return SAT_ERR_INVALID_ARG. A misaligned/out-of-range base
 * is SAT_ERR_INVALID_ARG, a base the cycle planner cannot fetch is
 * SAT_ERR_CAPACITY, and nothing changes on failure. */
sat_result_t sat_vdp2_layer_set_pattern_name_base(sat_vdp2_layer_t layer,
                                                  uint32_t byte_address);

/* Shows or hides a configured layer (hiding frees its VRAM reads). */
sat_result_t sat_vdp2_layer_set_enabled(sat_vdp2_layer_t layer, uint8_t enabled);

/* Priority 0-7; 0 hides the layer. Higher numbers are drawn in front; the
 * VDP1 sprite layer has its own (sat_vdp2_sprite_set_priority). */
sat_result_t sat_vdp2_layer_set_priority(sat_vdp2_layer_t layer, uint8_t priority);

/* Scroll position in 16.16 fixed point (the top-left dot shown). NBG0 and
 * NBG1 keep 8 fractional bits, NBG2 and NBG3 only the integer part.
 * The scroll reaches the VDP2 only in VBlank (a write during the picture would
 * apply from that scanline, tearing the layer; for NBG2/NBG3's vertical scroll
 * it even restarts the layer there): called mid-frame, it takes effect at the
 * next sat_vdp2_layers_commit(). The same holds for sat_vdp2_layer_set_zoom. */
sat_result_t sat_vdp2_layer_set_scroll(sat_vdp2_layer_t layer, int32_t x, int32_t y);

/* NBG0/NBG1 magnification in 16.16 fixed point: 0x10000 is 1:1, 0x20000
 * shows everything twice as large, 0x8000 half size. Reducing (below 1:1)
 * horizontally needs more VRAM reads per cycle and can be refused with
 * SAT_ERR_CAPACITY; the range is 1/4 to 8 times. */
sat_result_t sat_vdp2_layer_set_zoom(sat_vdp2_layer_t layer, uint32_t zoom_x, uint32_t zoom_y);

/* ------------------------------------------------------------------ */
/* RBG0: the rotation scroll screen, cell format                       */
/* ------------------------------------------------------------------ */

/* RBG0 draws a cell-format map through a 3x3 matrix, scaled per line by a
 * coefficient table: tilted floors, rotating and zooming maps (manual
 * chapter 6). It runs beside NBG0..NBG3 under this manager and takes whole
 * VRAM banks (RAMCTL) for its pattern names and its characters; the VDP2
 * ignores those banks' cycle patterns, so no NBG may read them, and a
 * configuration that leaves an NBG without its bank fails with
 * SAT_ERR_CAPACITY.
 *
 * The rotation parameter region holds TWO parameter sets (manual 6.3,
 * figure 6.4): A at the region base, 0x20 ignored bytes, B at base+0x80.
 * One address register (RPTA) covers the region; its bit 6 is ignored and
 * fixed to 0 for A and 1 for B, so the region's byte address must have
 * bit 7 clear. `parameter_mode` (RPMD) decides which set draws the screen:
 * A only, B only, or per-line A/B by the coefficient flag bit; window
 * switching (mode 3) needs the rotation window control register, which
 * this manager does not model, and is refused with SAT_ERR_INVALID_ARG.
 * While RBG0 is shown the CPU should write its banks only in blanking.
 *
 * Each set has its own coefficient table contract (KTCTL bits 4-0 for A,
 * 12-8 for B): per-line 2-word or 1-word entries in one of the four modes
 * below. Per-dot coefficients are not supported (they need a whole
 * rotation bank or Color RAM, manual 6.4). The map is 4x4 planes (A..P,
 * rows of four); outside it `over` applies. */
#define SAT_VDP2_ROTATION_OVER_REPEAT       0u   /* the map repeats */
#define SAT_VDP2_ROTATION_OVER_PATTERN      1u   /* over_pattern_name repeats */
#define SAT_VDP2_ROTATION_OVER_TRANSPARENT  2u
#define SAT_VDP2_ROTATION_OVER_512          3u   /* only 0..511 x 0..511 is drawn */

/* What a coefficient replaces. */
#define SAT_VDP2_COEFFICIENT_KXKY  0u
#define SAT_VDP2_COEFFICIENT_KX    1u
#define SAT_VDP2_COEFFICIENT_KY    2u
#define SAT_VDP2_COEFFICIENT_XP    3u

/* Which rotation parameter set draws the screen (RPMD, manual 6.3.5). */
#define SAT_VDP2_ROTATION_PARAMETER_A           0u  /* set A only */
#define SAT_VDP2_ROTATION_PARAMETER_B           1u  /* set B only */
#define SAT_VDP2_ROTATION_PARAMETER_COEFFICIENT 2u  /* per line: the coefficient
                                                     * flag bit picks the image */

/* Coefficient entry flags (figure 6.7 bit 15; both may be given). In
 * parameter mode A or B a flagged line is hidden; in mode COEFFICIENT a
 * flagged line shows the B image and the entry value still applies. */
#define SAT_VDP2_COEFFICIENT_HIDE   1u
#define SAT_VDP2_COEFFICIENT_USE_B  2u

/* The coefficient table contract of one parameter set. */
typedef struct sat_vdp2_rotation_coefficients {
    uint8_t enabled;     /* read a per-line coefficient table for this set */
    uint8_t mode;        /* SAT_VDP2_COEFFICIENT_* */
    uint8_t one_word;    /* 1: 1-word entries; 0: 2-word entries */
} sat_vdp2_rotation_coefficients_t;

/* One rotation parameter set's pattern name map: a 4x4 plane layout in cell
 * format. The RBG0 image uses the top-level plane_address/plane_pages_x/y/
 * over/over_pattern_name fields; RBG1 always reads its names from VRAM-B1
 * (manual p06_20) so its plane_address range must lie inside bank B1. */
typedef struct sat_vdp2_rotation_map {
    uint8_t plane_pages_x;       /* 1 or 2 */
    uint8_t plane_pages_y;       /* 1 or 2 (1x2 is prohibited, like RBG0) */
    uint8_t over;                /* SAT_VDP2_ROTATION_OVER_* */
    uint8_t reserved0;
    uint16_t over_pattern_name;  /* SAT_VDP2_ROTATION_OVER_PATTERN */
    uint16_t reserved1;
    /* Byte addresses of the 16 planes' pattern name tables. */
    uint32_t plane_address[16];
} sat_vdp2_rotation_map_t;

/* RBG1: the second rotation scroll screen (manual p06_20). It requires
 * RBG0 to display too, can never coexist with a normal scroll screen, and
 * its pattern names are fixed in VRAM-B1 with its character window in
 * VRAM-B0 (the manual's "the image data for each rotating scroll screen
 * must be stored in separate VRAMs"). The validation rules refuse any
 * configuration that breaks those constraints with SAT_ERR_INVALID_ARG.
 *
 * RBG1 is always drawn by rotation parameter set B (p06_30: RPTA's bit 6
 * is fixed to 1 for B), so RPMD must stay SAT_VDP2_ROTATION_PARAMETER_A
 * while RBG1 displays; live parameter-mode switches and concurrent NBG
 * configurations are refused with SAT_ERR_INVALID_ARG / SAT_ERR_BUSY.
 *
 * RBG1 shares parameter set B's 4x4 plane map with RBG0's B-mode -- the
 * VDP2 reads MPABRB..MPOPRB once and feeds both screens through it. The
 * validation insists on VRAM-B1 names when this map is read by RBG1. */
typedef struct sat_vdp2_rbg1_config {
    uint8_t enabled;             /* 1: show RBG1 */
    uint8_t color_mode;          /* 16, 256, 2048 or 32768 colours */
    uint8_t char_size;           /* sat_vdp2_char_size_t */
    uint8_t pattern_name_words;  /* 1 or 2 */
    uint8_t priority;            /* 0 (hidden) to 7 */
    uint8_t transparent;         /* 1: dot code 0 is transparent */
    uint8_t palette;             /* 1-word names: palette number bits 6-4 */
    uint8_t reserved0;
    uint32_t char_base_address;  /* 1-word names: window base, must sit in VRAM-B0 */
} sat_vdp2_rbg1_config_t;

typedef struct sat_vdp2_rotation_config {
    uint8_t color_mode;          /* 16, 256, 2048 or 32768 colours */
    uint8_t char_size;           /* sat_vdp2_char_size_t */
    uint8_t pattern_name_words;  /* 1 or 2 */
    uint8_t plane_pages_x;       /* plane size 1x1, 2x1 or 2x2 pages */
    uint8_t plane_pages_y;
    uint8_t priority;            /* 0 (hidden) to 7 */
    uint8_t transparent;         /* 1: dot code 0 is transparent */
    uint8_t palette;             /* 1-word names: palette number bits 6-4 */
    uint8_t over;                /* SAT_VDP2_ROTATION_OVER_* */
    uint8_t name_bank_mask;      /* SAT_VDP2_BANK_*: the banks of the pattern name tables */
    uint8_t char_bank_mask;      /* ... of the character data (not shared with the names) */
    uint8_t parameter_mode;      /* SAT_VDP2_ROTATION_PARAMETER_* */
    uint16_t over_pattern_name;  /* SAT_VDP2_ROTATION_OVER_PATTERN: a 1-word name */
    sat_vdp2_rotation_coefficients_t coefficients_a;
    sat_vdp2_rotation_coefficients_t coefficients_b;
    /* Byte addresses of the planes' pattern name tables, rules as for the NBGs. */
    uint32_t plane_address[16];
    uint32_t char_base_address;  /* 1-word names: as sat_vdp2_layer_config_t */
    /* The A+B parameter region: byte address, a multiple of 4 with bit 7
     * clear, outside the RBG0 banks. sat_vdp2_rotation_write_params fills
     * both sets. */
    uint32_t table_address;
    /* Parameter set B's pattern name map. Set B is always read at
     * region | 0x80 (manual p06_32), and RBG1 fetches B's names from
     * VRAM-B1. When rbg1.enabled is 0 the map may sit anywhere a name
     * table can; when it is 1 the validation insists on VRAM-B1. */
    sat_vdp2_rotation_map_t map_b;
    /* RBG1 sub-config; enabled = 0 leaves BGON R1ON clear. */
    sat_vdp2_rbg1_config_t rbg1;
} sat_vdp2_rotation_config_t;

/* Defaults: 256 colours, 1x1 characters, 1-word names, one page per plane,
 * priority 1, dot 0 transparent, the map repeating, parameter set A, no
 * coefficient tables, no parameter-set-B map, RBG1 hidden. The caller
 * sets the banks and addresses. */
void sat_vdp2_rotation_config_default(sat_vdp2_rotation_config_t* out_config);

/* RBG0 in bitmap format: one 512x256 or 512x512 picture that the rotation
 * parameters transform, instead of a pattern name map. It takes the place of
 * sat_vdp2_rotation_configure for the same screen, so the parameter region,
 * coefficient tables, priority and the other sat_vdp2_rotation_* calls work
 * as for the cell format; the picture's banks go to RBG0 and the NBGs use the
 * others. RBG1 cannot be shown with it (its data lives in VRAM-B too). */
typedef struct sat_vdp2_rotation_bitmap_config {
    uint8_t color_mode;          /* 16, 256, 2048 or 32768 colours */
    uint8_t height_512;          /* 1: 512x512, 0: 512x256 */
    uint8_t priority;            /* 0 (hidden) to 7 */
    uint8_t transparent;         /* 1: dot code 0 is transparent */
    uint8_t palette;             /* palette number bits 6-4 (16/256 colours) */
    uint8_t over;                /* REPEAT, TRANSPARENT or 512; PATTERN is refused */
    uint8_t parameter_mode;      /* SAT_VDP2_ROTATION_PARAMETER_* */
    uint8_t reserved0;
    sat_vdp2_rotation_coefficients_t coefficients_a;
    sat_vdp2_rotation_coefficients_t coefficients_b;
    /* Byte address of the picture, a multiple of 0x20000. The banks it covers
     * are given to RBG0; it must fit in VRAM. */
    uint32_t bitmap_address;
    /* The A+B parameter region: as for the cell format, outside the picture's banks. */
    uint32_t table_address;
} sat_vdp2_rotation_bitmap_config_t;

/* Defaults: 256 colours, 512x256, priority 1, dot 0 transparent, the picture
 * repeating, parameter set A, no coefficient tables. The caller sets the
 * addresses. */
void sat_vdp2_rotation_bitmap_config_default(sat_vdp2_rotation_bitmap_config_t* out_config);
/* The picture's size in bytes for its colour mode and height, the amount to
 * reserve as SAT_VDP2_RESERVE_ROTATION_BITMAP; 0 for a bad colour mode. */
uint32_t sat_vdp2_rotation_bitmap_bytes(const sat_vdp2_rotation_bitmap_config_t* config);
sat_result_t sat_vdp2_rotation_bitmap_configure(const sat_vdp2_rotation_bitmap_config_t* config);

sat_result_t sat_vdp2_rotation_configure(const sat_vdp2_rotation_config_t* config);
/* Hides RBG0 and gives its banks back to the NBGs. */
sat_result_t sat_vdp2_rotation_release(void);
sat_result_t sat_vdp2_rotation_set_enabled(uint8_t enabled);
sat_result_t sat_vdp2_rotation_set_priority(uint8_t priority);

/* Changes the parameter mode live (RPMD): SAT_VDP2_ROTATION_PARAMETER_A, B
 * or COEFFICIENT. The window mode is not supported: SAT_ERR_INVALID_ARG.
 * While RBG1 is visible the parameter mode must stay A (manual p06_30,
 * p06_35: RBG1 reads parameter set B only) - refused with
 * SAT_ERR_INVALID_ARG. */
sat_result_t sat_vdp2_rotation_set_parameter_mode(uint8_t mode);

/* Shows or hides RBG1 without a full reconfigure. SAT_ERR_INVALID_ARG when
 * RBG1 is configured and the rotation parameter mode is not A. */
sat_result_t sat_vdp2_rotation_rbg1_set_enabled(uint8_t enabled);

/* One rotation parameter set (manual 6.3), used for A and B alike. The
 * screen dot (H, V) shows map dot
 *   X = kx * (Xsp + dX * H) + Xp,   Y = ky * (Ysp + dY * H) + Yp
 * with (Xs, Ys, Zs) = (xst + dxst * V, yst + dyst * V, zst) and
 *   Xsp = A (Xs - px) + B (Ys - py) + C (Zs - pz),  dX = A dx + B dy
 *   Ysp = D (Xs - px) + E (Ys - py) + F (Zs - pz),  dY = D dx + E dy
 *   Xp  = A (px - cx) + B (py - cy) + C (pz - cz) + cx + mx    (Yp alike)
 * Values are 16.16 fixed point; the VDP2 keeps 10 fraction bits of most.
 * The set's own coefficient table (when its contract is enabled in the
 * configuration) starts at `coefficient_address` and advances
 * `coefficient_step` entries per line. */
typedef struct sat_vdp2_rotation_params {
    int32_t xst, yst, zst;
    int32_t dxst, dyst;          /* per line, -4 < d < 4 */
    int32_t dx, dy;              /* per dot, -4 < d < 4 */
    int32_t matrix[6];           /* A B C D E F, -8 < m < 8 */
    int16_t px, py, pz;
    int16_t cx, cy, cz;
    int32_t mx, my;
    int32_t kx, ky;              /* used where the coefficients do not replace them */
    uint32_t coefficient_address;  /* byte address of the first entry: even, a
                                    * multiple of 4 when the contract is 2-word */
    int32_t coefficient_step;    /* 16.16 entries per line (0x10000: one each) */
} sat_vdp2_rotation_params_t;

/* Writes the whole A+B region - A at `table_address`, B at +0x80 - and makes
 * it the one the VDP2 reads from the next sat_vdp2_layers_commit() on. Both
 * sets are always written together, so B can never be stale. The coefficient
 * entry offsets in KTAOF come from each set's enabled contract in the
 * configuration. A region being read while it is rewritten tears the
 * picture: alternate between two. */
sat_result_t sat_vdp2_rotation_write_params(uint32_t table_address,
                                            const sat_vdp2_rotation_params_t* params_a,
                                            const sat_vdp2_rotation_params_t* params_b);

/* A 2-word coefficient entry as the VDP2 reads it (figure 6.7): a 16.16
 * value between -128 and 128 (saturated) plus the flag bits, which hide the
 * line or, in parameter mode COEFFICIENT, show the B image for that line. */
uint32_t sat_vdp2_rotation_coefficient_2word(int32_t value, uint8_t flags);

/* The words sat_vdp2_rotation_write_params stores, without writing VRAM or
 * changing which table the VDP2 reads: for a program that copies them into
 * the table it already uses, in VBlank (see sat_vdp1_set_swap_handler). */
#define SAT_VDP2_ROTATION_PARAM_WORDS 48u
sat_result_t sat_vdp2_rotation_encode_params(const sat_vdp2_rotation_params_t* params, uint16_t* words);

/* Compatibility name for sat_vdp2_rotation_coefficient_2word: `transparent`
 * is the HIDE flag. */
uint32_t sat_vdp2_rotation_coefficient(int32_t value, uint8_t transparent);

/* A 1-word coefficient entry (figure 6.7): the same flag bit in bit 15,
 * sign bit 14, a 16.16 value saturated to just under +-16. */
uint16_t sat_vdp2_rotation_coefficient_1word(int32_t value, uint8_t flags);

/* Writes `count` 2-word coefficients (as sat_vdp2_rotation_coefficient_2word
 * returns) at a byte address, a multiple of 4. */
sat_result_t sat_vdp2_rotation_write_coefficients(uint32_t byte_address, const uint32_t* coefficients,
                                                  uint32_t count);

/* Writes `count` 1-word coefficients at an even byte address. */
sat_result_t sat_vdp2_rotation_write_coefficients_1word(uint32_t byte_address, const uint16_t* coefficients,
                                                        uint32_t count);

/* ------------------------------------------------------------------ */
/* Raster effects: tables the VDP2 reads while it draws each line       */
/* ------------------------------------------------------------------ */

/* Line scroll (NBG0, NBG1): a table in VRAM gives, for every line or group of
 * 2, 4 or 8 lines, any of a horizontal scroll, a vertical scroll and a
 * horizontal coordinate increment (zoom), added to the layer's own scroll
 * registers. An entry holds the enabled fields in that order, two 16-bit
 * words each. The table needs no cycle pattern, but it must not overlap
 * anything else in VRAM. */
typedef struct sat_vdp2_line_scroll_config {
    uint8_t layer;           /* SAT_VDP2_NBG0 or SAT_VDP2_NBG1, already configured */
    uint8_t horizontal;      /* entries carry a horizontal scroll */
    uint8_t vertical;        /* ... a vertical scroll */
    uint8_t zoom;            /* ... a horizontal coordinate increment */
    uint8_t interval;        /* 0..3: a new entry every 1, 2, 4 or 8 lines */
    uint8_t reserved0;
    uint16_t reserved1;
    uint32_t table_address;  /* byte address in VDP2 VRAM, even */
} sat_vdp2_line_scroll_config_t;

typedef struct sat_vdp2_line_scroll_entry {
    int32_t x;               /* horizontal scroll, 16.16 fixed point */
    int32_t y;               /* vertical scroll, 16.16 */
    uint32_t zoom;           /* horizontal coordinate increment, 16.16 (0x10000: 1:1) */
} sat_vdp2_line_scroll_entry_t;

/* Bytes a table needs for `lines` display lines. */
uint32_t sat_vdp2_line_scroll_table_bytes(const sat_vdp2_line_scroll_config_t* config, uint32_t lines);

sat_result_t sat_vdp2_layer_line_scroll_enable(const sat_vdp2_line_scroll_config_t* config);
sat_result_t sat_vdp2_layer_line_scroll_disable(sat_vdp2_layer_t layer);

/* Writes entries (only the enabled fields are stored) starting at entry
 * `first_entry`. */
sat_result_t sat_vdp2_line_scroll_write(sat_vdp2_layer_t layer, uint32_t first_entry,
                                        const sat_vdp2_line_scroll_entry_t* entries, uint32_t count);

/* The words sat_vdp2_line_scroll_write would store for `count` entries, into
 * `words` (room for `capacity`), without writing VRAM. Returns the number of
 * words, or 0 for invalid arguments, a layer without line scroll or too
 * little room. */
uint32_t sat_vdp2_line_scroll_encode(sat_vdp2_layer_t layer, const sat_vdp2_line_scroll_entry_t* entries,
                                     uint32_t count, uint16_t* words, uint32_t capacity);

/* Fills the whole table for `lines` lines with a horizontal sine wave:
 * `amplitude` in 16.16 dots, one period every `period_lines` lines, starting
 * at `phase_degrees` (16.16). The other enabled fields get 0 and 1:1. The
 * classic water and heat-haze wobble, at no CPU cost per frame beyond
 * rewriting the phase. */
sat_result_t sat_vdp2_line_scroll_fill_wave(sat_vdp2_layer_t layer, uint32_t lines,
                                            int32_t amplitude, uint32_t period_lines,
                                            int32_t phase_degrees);

/* Vertical cell scroll: one vertical scroll per 8-dot column (the layer must
 * have been configured with vertical_cell_scroll = 1 and a table address).
 * NBG0 and NBG1 share one table, their entries alternating: the layer decides
 * which slot is written. Values are 16.16, relative to the layer's scroll. */
sat_result_t sat_vdp2_vertical_cell_scroll_write(sat_vdp2_layer_t layer, uint32_t first_cell,
                                                 const int32_t* values, uint32_t count);

/* Back screen colour per line (RGB555, one word per line, at least the
 * display height). The single-colour sat_vdp2_set_backdrop_color, and the
 * sat_app_frame_begin helper that calls it every frame, switch back to one
 * colour. */
sat_result_t sat_vdp2_back_screen_set_lines(uint32_t table_address, const uint16_t* rgb555,
                                            uint32_t count);

/* Line colour screen: a colour RAM index per line, used as the colour a layer
 * is blended with. The blend needs colour calculation enabled for that layer;
 * sat_vdp2_layer_set_line_color_insert selects which layers get it. */
sat_result_t sat_vdp2_line_color_screen_set(uint32_t table_address, const uint16_t* cram_indices,
                                            uint32_t count);
sat_result_t sat_vdp2_layer_set_line_color_insert(sat_vdp2_layer_t layer, uint8_t enabled);

/* The line colour screen inserted under the sprite layer (LNCLEN SPLCEN): it
 * becomes the image a sprite dot is mixed with, wherever the sprite is the
 * top image, so screens without sprites in front never receive it. With
 * sprite colour calculation in add mode this adds a per-line colour (a light)
 * to the sprites only; see sat_vdp2_hires_sprite_color_calc_set. Replayed by
 * sat_vdp2_layers_commit(). */
sat_result_t sat_vdp2_sprite_set_line_color_insert(uint8_t enabled);

/* The eight cycle pattern registers last written: A0L A0U A1L A1U B0L B0U
 * B1L B1U. For tests and debugging. */
sat_result_t sat_vdp2_layer_cycle_patterns(uint16_t out_registers[8]);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_LAYERS_H */
