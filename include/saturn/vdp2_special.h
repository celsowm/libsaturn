#ifndef SATURN_VDP2_SPECIAL_H
#define SATURN_VDP2_SPECIAL_H

#include <stdint.h>

#include "saturn/vdp2_layers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* VDP2 special priority (manual 11.2) and special colour calculation (manual
 * 12.2). Both change only the least significant bit of the priority number or
 * the colour-calculation enable, per character or per dot, instead of for the
 * whole screen. The registers are write-only; the library owns SFPRMD, SFCCMD,
 * SFCODE and SFSEL once one of these calls succeeds and replays them from
 * sat_vdp2_layers_commit().
 *
 * The layer must already be configured with sat_vdp2_layer_configure(): the
 * setters validate the mode against its pattern-name size, bitmap flag and
 * colour format.
 *
 * Per-character and per-dot modes read the PR (special priority) and CC
 * (special colour calculation) bits of a 2-word pattern name. Use
 * sat_vdp2_pattern_name_2word() to build one; the library keeps the whole
 * 32-bit name so both words are emitted in the manual's order. The 1-word
 * PNCN auxiliary bits and the bitmap BMPNA bits are not exposed, so those
 * layers accept mode 0 only.
 *
 * Per-dot modes need sat_vdp2_special_function_code_set() and, per layer,
 * sat_vdp2_special_function_select(). Bit n of a code register covers dot
 * colour codes 2n and 2n+1 (manual 10.3). The special colour/priority modes
 * other than 0 are valid only while that screen is the top image; the
 * hardware ignores them for any other image, so the caller owns that
 * composition choice. Per-dot modes are prohibited for RGB colour formats. */

#define SAT_VDP2_SPECIAL_PRIORITY_SCREEN 0u       /* use PRINA/PRINB as-is */
#define SAT_VDP2_SPECIAL_PRIORITY_CHARACTER 1u    /* PR bit of the 2-word name */
#define SAT_VDP2_SPECIAL_PRIORITY_DOT 2u          /* PR=1 and matching SFCODE */

#define SAT_VDP2_SPECIAL_COLOR_SCREEN 0u          /* CCCTL enable only */
#define SAT_VDP2_SPECIAL_COLOR_CHARACTER 1u       /* CC bit of the 2-word name */
#define SAT_VDP2_SPECIAL_COLOR_DOT 2u             /* CC=1 and matching SFCODE */
#define SAT_VDP2_SPECIAL_COLOR_MSB 3u             /* CRAM colour bit 15 */

/* Special priority mode (SFPRMD) for one NBG screen. Mode 3 is
 * setting-prohibited. Returns SAT_ERR_INVALID_ARG for a bad layer or mode and
 * SAT_ERR_UNSUPPORTED when the layer's name size / bitmap format cannot carry
 * the selected mode. */
sat_result_t sat_vdp2_special_priority_mode_set(sat_vdp2_layer_t layer, uint8_t mode);

/* Special colour-calculation mode (SFCCMD) for one NBG screen. Mode 3 selects
 * colour calculation by the CRAM entry's most significant bit (bit 15);
 * SAT_RGB555 colors have it set, SAT_RGB555_RAW colors do not. Returns the
 * same errors as the priority setter. */
sat_result_t sat_vdp2_special_color_calc_mode_set(sat_vdp2_layer_t layer, uint8_t mode);

/* SFCODE: two 8-bit masks, A and B. The per-dot modes read the mask selected
 * for the layer with sat_vdp2_special_function_select(). */
sat_result_t sat_vdp2_special_function_code_set(uint8_t code_a, uint8_t code_b);

/* SFSEL for one NBG screen: 0 selects code A, 1 selects code B. */
sat_result_t sat_vdp2_special_function_select(sat_vdp2_layer_t layer, uint8_t select);

/* Builds one 2-word pattern name (manual figure 4.9). The returned value is
 * the 32-bit name in big-endian order: the auxiliary word (vertical flip,
 * horizontal flip, special priority, special colour calculation and the
 * 7-bit palette number) in bits 31-16, then the 15-bit character number in
 * bits 15-0. Write the high word first, then the low word:
 *
 *   uint32_t name = sat_vdp2_pattern_name_2word(char_number, palette, ...);
 *   map[i * 2 + 0] = (uint16_t)(name >> 16);
 *   map[i * 2 + 1] = (uint16_t)name;
 *
 * The palette is 7 bits (0-127), the character number 15 bits. */
static inline uint32_t sat_vdp2_pattern_name_2word(uint16_t char_number, uint8_t palette,
                                                   uint8_t flip_h, uint8_t flip_v,
                                                   uint8_t special_priority,
                                                   uint8_t special_color) {
    const uint16_t aux = (uint16_t)(((flip_v & 1u) << 15u) | ((flip_h & 1u) << 14u) |
                                    ((special_priority & 1u) << 13u) |
                                    ((special_color & 1u) << 12u) | (palette & 0x7Fu));
    return ((uint32_t)aux << 16u) | (uint32_t)(char_number & 0x7FFFu);
}

#ifdef __cplusplus
}
#endif

#endif
