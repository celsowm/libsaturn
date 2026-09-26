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

/* Shows or hides a configured layer (hiding frees its VRAM reads). */
sat_result_t sat_vdp2_layer_set_enabled(sat_vdp2_layer_t layer, uint8_t enabled);

/* Priority 0-7; 0 hides the layer. Higher numbers are drawn in front; the
 * VDP1 sprite layer has its own (sat_vdp2_sprite_set_priority). */
sat_result_t sat_vdp2_layer_set_priority(sat_vdp2_layer_t layer, uint8_t priority);

/* Scroll position in 16.16 fixed point (the top-left dot shown). NBG0 and
 * NBG1 keep 8 fractional bits, NBG2 and NBG3 only the integer part. */
sat_result_t sat_vdp2_layer_set_scroll(sat_vdp2_layer_t layer, int32_t x, int32_t y);

/* NBG0/NBG1 magnification in 16.16 fixed point: 0x10000 is 1:1, 0x20000
 * shows everything twice as large, 0x8000 half size. Reducing (below 1:1)
 * horizontally needs more VRAM reads per cycle and can be refused with
 * SAT_ERR_CAPACITY; the range is 1/4 to 8 times. */
sat_result_t sat_vdp2_layer_set_zoom(sat_vdp2_layer_t layer, uint32_t zoom_x, uint32_t zoom_y);

/* The eight cycle pattern registers last written: A0L A0U A1L A1U B0L B0U
 * B1L B1U. For tests and debugging. */
sat_result_t sat_vdp2_layer_cycle_patterns(uint16_t out_registers[8]);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_LAYERS_H */
