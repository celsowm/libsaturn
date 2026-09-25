#ifndef SATURN_VDP2_COLOR_CALC_H
#define SATURN_VDP2_COLOR_CALC_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Saturn VDP2 sprite color-calculation configuration for LibSaturn's 16-bit
 * palette Sprite Type 0 path.
 *
 * `normal_priority` is the priority used by ordinary LibSaturn sprites.
 * While color calculation is enabled, priority selector 1 is reserved for
 * faded sprites and receives normal_priority - 1. The VDP2 color-calculation
 * condition is equality against that reserved priority. This keeps ordinary
 * sprites/HUD at selector 0 unaffected while a marked sprite can select one
 * of ratio[0..7].
 *
 * Enabling therefore requires normal_priority in 2..7. Put any background
 * that must remain behind both kinds of sprite below normal_priority - 1.
 *
 * Hardware ratio values are 0..31: 0 is essentially all top/sprite image;
 * 31 is all second/background image. */
typedef struct sat_vdp2_sprite_color_calc_config {
    uint8_t enabled;
    uint8_t normal_priority;
    uint8_t ratio[8];
} sat_vdp2_sprite_color_calc_config_t;

sat_result_t sat_vdp2_sprite_color_calc_configure(
    const sat_vdp2_sprite_color_calc_config_t* config
);

sat_result_t sat_vdp2_sprite_color_calc_set_ratio(
    uint8_t slot,
    uint8_t ratio
);

/* Replays SPCTL/PRISA/CCCTL/CCRSA-D from LibSaturn's color-calc state.
 * The ordinary and faded priority selectors are also registered with the
 * generic VDP2 layer shadow at configure/disable time. This ensures that
 * sat_vdp2_layers_commit() replays the same PRISA word each frame; an
 * extra color-calc commit after every layer commit is NOT needed, and doing
 * so after the VBlank window can defer the next gameplay frame.
 * Call this explicit commit only when restoring color-calc registers or
 * when deliberately changing the configuration. Outside VBlank it waits for
 * the next VBlank start before writing latched registers. */
/* Standard alpha preset: 8 shared slots with ratios {0,4,8,12,16,20,24,31}.
 * Overrides any distance-fade or other consumer's ratio table. Priority 2..7
 * must put both kinds of sprites above the VDP2 background to be blended. */
sat_result_t sat_vdp2_sprite_color_calc_configure_alpha(uint8_t normal_priority);

/* Pick the nearest *existing* slot for alpha 1..254 without changing the
 * global eight-ratio table. Hardware ratios are 1/32 steps and the table holds
 * only eight of them, so the requested alpha is snapped: out_alpha (optional)
 * receives the alpha the hardware really shows, (31 - ratio) * 8 (ratio 16 of
 * the alpha preset gives 120 for a requested 128). Returns NOT_INITIALIZED if
 * color calc is disabled. 0 (skip) and 255 (opaque) are handled by the caller.
 * Under sat_vdp2_sprite_color_calc_set_strict_alpha(1) a slot more than 2
 * hardware ratio units from the request returns UNSUPPORTED instead. */
sat_result_t sat_vdp2_sprite_color_calc_alpha_slot(
    uint8_t alpha, uint8_t* out_slot, uint8_t* out_alpha);

/* Opt-in (default 0): refuse, rather than snap, alphas no slot approximates
 * within 2 hardware ratio units. Applies to sat_draw_texture's ALPHA blend. */
sat_result_t sat_vdp2_sprite_color_calc_set_strict_alpha(uint8_t strict);

sat_result_t sat_vdp2_sprite_color_calc_commit(void);

sat_result_t sat_vdp2_sprite_color_calc_disable(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_VDP2_COLOR_CALC_H */
