#ifndef BH_RENDER_H
#define BH_RENDER_H

/* Turns the simulation into a list of VDP1 sprite instances in painter's order (later draws on
 * top): pickups, enemies, options, ship, player shots, enemy bullets, explosions, HUD. No
 * hardware is touched here; main.c hands the list to sat_sprite_batch_draw. */

#include <stdint.h>

#include "bh_sim.h"
#include "bh_view.h"
#include "saturn/sprite_batch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Worst case: 1024 bullets + 96 shots + 24 enemies + 40 explosions + 16 pickups + 3 ship parts
 * + the HUD (about 150 glyphs and pills). */
#define BH_MAX_INSTANCES 1400u

typedef struct bh_hud {
    uint32_t fps;            /* shown top right */
    uint32_t bullets;        /* bullets alive, same line */
    uint32_t stage;          /* 1-based */
    uint8_t show_timing;     /* draw the CPU-time line (benchmark builds) */
    uint32_t ticks[9];       /* mean FRT ticks per frame over the last second: service, sim, build, draw, end, then the
                              service parts: vdp2, pad, disc, audio */
} bh_hud_t;

/* Appends to out[0..capacity) and returns the count. Frames that are not resident are left out
 * (frame_ready[i] == 0), so a group that is still streaming never draws garbage. */
uint32_t bh_render_build(const bh_sim_t* sim, const bh_view_t* view, const uint8_t* frame_ready,
                         const bh_hud_t* hud, sat_sprite_instance_t* out, uint32_t capacity);

/* Atlas frame index of a bullet; exposed for the host test and for radius lookup. */
uint16_t bh_frame_bullet(uint8_t shape, uint8_t color);
uint16_t bh_frame_enemy(uint16_t stage, uint8_t type);
uint16_t bh_frame_explosion(uint8_t index);
/* The atlas group (residency group) an enemy set of a stage lives in. */
uint8_t bh_stage_group(uint16_t stage);

#ifdef __cplusplus
}
#endif
#endif
