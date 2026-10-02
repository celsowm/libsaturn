#ifndef HSP_ART_H
#define HSP_ART_H

/* The example's art, drawn by code: nothing here is a copy of any game's graphics. Terrain cells
 * come from the terrain profiles themselves (what is solid is what is painted), the sprite sheet
 * is built from rectangles and circles. */

#include <stdint.h>

#include "saturn/surface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HSP_SHEET_W 256
#define HSP_SHEET_H 96

/* The VDP2 palette numbers of the three scroll layers (CRAM entries 16 * number). */
#define HSP_PALETTE_FOREGROUND 1u
#define HSP_PALETTE_HILLS 2u
#define HSP_PALETTE_CLOUDS 3u

/* Number of 8 x 8 characters of the cell sheet (profiles followed by the decor characters). */
#define HSP_CHAR_COUNT 18u

/* Fills the 4-bit cell sheet: HSP_CHAR_COUNT characters of 16 words each, character n at
 * words[16 * n]. `out` must hold 16 * HSP_CHAR_COUNT words. */
void hsp_art_cells(uint16_t* out);

/* The three 16-colour palettes, written one after the other (16 entries each, index 0 unused). */
void hsp_art_palettes(uint16_t* out48);

/* Draws the sprite sheet (indexed, 256 colours) and describes it as a surface. The pixels live in
 * static storage of this module, so the surface can back a persistent texture. */
sat_result_t hsp_art_sheet(sat_surface_t* out);

#ifdef __cplusplus
}
#endif

#endif /* HSP_ART_H */
