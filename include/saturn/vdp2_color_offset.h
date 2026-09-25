#ifndef SATURN_VDP2_COLOR_OFFSET_H
#define SATURN_VDP2_COLOR_OFFSET_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* VDP2 colour offset (VDP2 manual chapter 13): a signed value added to every
 * output pixel of the chosen layers, per channel, clamped to 0..255. It runs
 * after colour calculation, costs nothing per pixel, and is the Saturn's
 * layer-wide fade, darken (SUBTRACT) and tint:
 *
 *   out = clamp(colour + offset, 0, 255)   for r, g and b separately.
 *
 * It is additive, not a multiply: a negative offset takes the same amount
 * off dark and bright pixels alike. Two offset values exist (A and B); each
 * layer uses one of them or none. The layers are VDP2 layers, and SPRITE is
 * the whole VDP1 framebuffer as one layer: an offset cannot single out one
 * sprite (see sat_draw_texture's tint for that).
 *
 * The registers are write-only and latched at VBlank; the library keeps
 * shadows that sat_vdp2_layers_commit() replays every frame. */

#define SAT_VDP2_LAYER_NBG0   0x01u /* or RBG1 */
#define SAT_VDP2_LAYER_NBG1   0x02u /* or EXBG */
#define SAT_VDP2_LAYER_NBG2   0x04u
#define SAT_VDP2_LAYER_NBG3   0x08u
#define SAT_VDP2_LAYER_RBG0   0x10u
#define SAT_VDP2_LAYER_BACK   0x20u
#define SAT_VDP2_LAYER_SPRITE 0x40u
#define SAT_VDP2_LAYER_ALL    0x7Fu

typedef enum sat_vdp2_color_offset_bank {
    SAT_VDP2_COLOR_OFFSET_A = 0,
    SAT_VDP2_COLOR_OFFSET_B = 1
} sat_vdp2_color_offset_bank_t;

/* Each channel -256..255 (9-bit signed hardware value). */
typedef struct sat_vdp2_color_offset {
    int16_t r;
    int16_t g;
    int16_t b;
} sat_vdp2_color_offset_t;

/* Sets offset A or B. Layers already using that bank change with it. */
sat_result_t sat_vdp2_color_offset_set(
    sat_vdp2_color_offset_bank_t bank,
    const sat_vdp2_color_offset_t* offset
);

/* Makes the layers in layer_mask use `bank`; other layers keep theirs. */
sat_result_t sat_vdp2_color_offset_enable(
    uint8_t layer_mask,
    sat_vdp2_color_offset_bank_t bank
);

/* Stops applying any offset to the layers in layer_mask. */
sat_result_t sat_vdp2_color_offset_disable(uint8_t layer_mask);

/* The offset that tints white to `r, g, b` (offset = target - 255, so every
 * channel only ever darkens). An additive stand-in for a multiply: exact on
 * white, and it takes the same amount off darker colours too. */
static inline sat_vdp2_color_offset_t sat_vdp2_color_offset_tint(
    uint8_t r, uint8_t g, uint8_t b) {
    sat_vdp2_color_offset_t offset;
    offset.r = (int16_t)((int16_t)r - 255);
    offset.g = (int16_t)((int16_t)g - 255);
    offset.b = (int16_t)((int16_t)b - 255);
    return offset;
}

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_COLOR_OFFSET_H */
