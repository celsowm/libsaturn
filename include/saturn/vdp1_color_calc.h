#ifndef SATURN_VDP1_COLOR_CALC_H
#define SATURN_VDP1_COLOR_CALC_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/vdp1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Select saturating VDP2 addition instead of a ratio slot. The mode is
 * shared by the whole frame; mixing ADD and ratio draws returns BUSY.
 * ADD requires enabled sprite color calculation; otherwise NOT_INITIALIZED.
 * Only the VDP2 image below the sprite participates, not overwritten VDP1
 * pixels. RGB/LUT4 materials cannot carry this indexed selector. */
#define SAT_VDP2_SPRITE_ADD 253u

/* Draw one color-bank sprite through VDP2 sprite color calculation.
 *
 * These helpers target the Saturn's 16-bit palette Sprite Type 0 layout used
 * by LibSaturn's indexed8 textures. The caller chooses one of the eight VDP2
 * color-calculation ratio slots (0..7), or SAT_VDP2_SPRITE_ADD;
 * palette/priority/ratio bit packing is
 * kept private to LibSaturn.
 *
 * Configure the ratio table with sat_vdp2_sprite_color_calc_configure() first.
 * Existing sat_draw_sprite* calls remain normal/opaque with respect to VDP2
 * color calculation. */
sat_result_t sat_draw_sprite_color_calc(
    const sat_sprite_cmd_t* cmd,
    uint8_t color_calc_slot
);

sat_result_t sat_draw_sprite_scaled_color_calc(
    const sat_scaled_sprite_cmd_t* cmd,
    uint8_t color_calc_slot
);

sat_result_t sat_draw_sprite_distorted_color_calc(
    const sat_distorted_sprite_cmd_t* cmd,
    uint8_t color_calc_slot
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP1_COLOR_CALC_H */
