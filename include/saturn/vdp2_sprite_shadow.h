#ifndef SATURN_VDP2_SPRITE_SHADOW_H
#define SATURN_VDP2_SPRITE_SHADOW_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* VDP2 sprite shadow function (manual 14.1, p14_10).
 *
 * The shadow function makes a sprite transparent and halves the brightness of
 * the top image underneath it. There are two kinds of shadow sprite:
 *
 *   normal shadow   A palette dot whose low colour-data bits are all 1 except
 *                   the LSB, which is 0 (manual figure 14.3). It can cast on
 *                   the scroll screens and the back screen selected by SDCTL,
 *                   never on another sprite.
 *   MSB shadow      A palette dot with bit 15 set. With the other 15 bits all
 *                   zero it is a transparent shadow and casts on the selected
 *                   scroll/back screen; with any of them non-zero it is a
 *                   sprite shadow and darkens the sprite dot already in the
 *                   frame buffer. VDP1's MSB-on command produces the latter
 *                   (see SAT_SPRITE_FLAG_MSB_ON).
 *
 * This module pins the sprite layer to type 2 while enabled: palette-only,
 * 16-bit dots, one priority-selector bit (S0PRIN/S1PRIN) and three colour
 * calculation bits. Type 2 is a type both shadow forms can use.
 * While shadow mode is active every visible VDP1 dot must be palette format
 * (bit 15 clear before MSB-on); RGB dots would be read as shadow/window data.
 * The configure call therefore refuses hi-res/8-bit sprite modes, and refuses
 * to coexist with the palette-priority/window/colour-calculation features or
 * the VDP1 sprite colour-calculation module, which also own SPCTL/PRISA.
 * Calling either side while the other is active returns SAT_ERR_BUSY.
 *
 * The shadow words are written to the VDP1 frame buffer through a LUT4
 * texture: put the encoded word in a 16-entry LUT (sat_vdp1_upload_lut),
 * upload the silhouette with sat_tex_upload_lut4_pixels, and draw it with the
 * normal sprite calls. Dot code 0 stays transparent. The library cannot
 * express these words through CMDCOLR's palette<<8 form, which is why the LUT
 * is the supported path. */

#define SAT_VDP2_SHADOW_MODE_OFF 0u
#define SAT_VDP2_SHADOW_MODE_NORMAL 1u
#define SAT_VDP2_SHADOW_MODE_MSB 2u

/* SDCTL target bits: a normal shadow or a transparent MSB shadow only
 * darkens the selected screens. A sprite MSB shadow (MSB-on over an existing
 * sprite dot) has no SDCTL bit; the manual lets it darken sprites always. */
#define SAT_VDP2_SHADOW_TARGET_NBG0 0x01u
#define SAT_VDP2_SHADOW_TARGET_NBG1 0x02u
#define SAT_VDP2_SHADOW_TARGET_NBG2 0x04u
#define SAT_VDP2_SHADOW_TARGET_NBG3 0x08u
#define SAT_VDP2_SHADOW_TARGET_RBG0 0x10u
#define SAT_VDP2_SHADOW_TARGET_BACK 0x20u
#define SAT_VDP2_SHADOW_TARGET_ALL 0x3Fu

typedef struct sat_vdp2_sprite_shadow_config {
    uint8_t mode;             /* SAT_VDP2_SHADOW_MODE_*; OFF disables */
    uint8_t target_mask;      /* SAT_VDP2_SHADOW_TARGET_* bits, 0-0x3F */
    uint8_t transparent_shadow; /* TPSDSL: allow transparent MSB shadows */
    uint8_t base_priority;    /* priority[0] for ordinary sprite dots, 0-7 */
    uint8_t shadow_priority;  /* priority[1] for selector-1 shadow dots, 0-7 */
    uint8_t reserved[3];
} sat_vdp2_sprite_shadow_config_t;

/* Enables the VDP2 shadow function and selects type-2 palette sprites.
 * `mode` OFF clears SDCTL and restores the normal sprite mode, exactly like
 * sat_vdp2_sprite_shadow_disable(). `target_mask` may be zero: the shadow
 * sprites are then drawn but darken nothing, which is useful as an A/B
 * control. `transparent_shadow` is TPSDSL and only matters for MSB mode.
 *
 * Returns SAT_ERR_INVALID_ARG for a mode/target/priority outside the defined
 * ranges, SAT_ERR_UNSUPPORTED for hi-res or 8-bit sprite modes, and
 * SAT_ERR_BUSY while palette priorities, the palette window or palette colour
 * calculation are active. The configuration is not changed on failure. */
sat_result_t sat_vdp2_sprite_shadow_configure(const sat_vdp2_sprite_shadow_config_t* config);

/* Clears SDCTL, turns the shadow mode off and restores SPCTL/PRISA to the
 * ordinary sprite mode. Safe when already off. */
sat_result_t sat_vdp2_sprite_shadow_disable(void);

/* Whether the type-2 shadow configuration is currently active. */
uint8_t sat_vdp2_sprite_shadow_active(void);

/* Normal-shadow dot word for type 2 (manual figure 14.3). priority_selector
 * is bit 14 (only 0 or 1 is meaningful); color_calc_selector occupies bits
 * 13-11. The low 11 colour-data bits are 0x7FE, so the canonical selector-1
 * word is 0x47FE. Bit 15 stays 0: the normal-shadow condition is decided from
 * the colour-data bits alone. */
static inline uint16_t sat_vdp2_shadow_normal_word(uint8_t priority_selector,
                                                   uint8_t color_calc_selector) {
    return (uint16_t)(((priority_selector & 1u) << 14u) |
                      ((color_calc_selector & 7u) << 11u) | 0x07FEu);
}

/* Transparent MSB-shadow dot word for type 2: bit 15 set and all other 15
 * bits zero (manual 14.1). It therefore uses priority register 0 (S0PRIN),
 * not selector 1; give that register the required priority. The screen must
 * select TPSDSL and its SDCTL target bit, or nothing is darkened. */
static inline uint16_t sat_vdp2_shadow_transparent_msb_word(void) {
    return 0x8000u;
}

/* The dot word VDP1 MSB-on writes: set bit 15 and keep the existing 15 bits.
 * If `sprite_dot` is a normal palette dot already in the frame buffer, the
 * result is a sprite shadow; if its low 15 bits are zero, a transparent
 * shadow. This is also exactly what SAT_SPRITE_FLAG_MSB_ON does in VDP1. */
static inline uint16_t sat_vdp2_shadow_msb_word(uint16_t sprite_dot) {
    return (uint16_t)(sprite_dot | 0x8000u);
}

#ifdef __cplusplus
}
#endif

#endif
