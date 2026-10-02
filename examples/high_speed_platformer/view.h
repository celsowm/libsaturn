#ifndef HSP_VIEW_MODULE_H
#define HSP_VIEW_MODULE_H

/* Everything that touches the video hardware: the VDP2 scroll layers fed by stage_map2, the
 * sprite sheet, and the VDP1 draw calls. The game itself (game.h) never sees any of it. */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/font.h"
#include "saturn/stage_map2.h"
#include "saturn/texture.h"

#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HSP_STAGING_WORDS 12288u
#define HSP_STAGING_RUNS 512u

typedef struct hsp_view {
    sat_texture_t sheet;
    sat_ascii_font_t font;
    sat_stage_map2_t map;
    uint16_t staging_words[HSP_STAGING_WORDS];
    sat_stage_map2_run_t staging_runs[HSP_STAGING_RUNS];
    uint8_t ready;
} hsp_view_t;

/* Uploads the cells and palettes, configures the three scroll layers, creates the sprite sheet
 * texture and prepares the clip regions. Call once, after sat_app_init_default. */
sat_result_t hsp_view_init(hsp_view_t* view);

/* Right after VBlank starts: writes the cells staged last frame and moves the layers. */
sat_result_t hsp_view_present(hsp_view_t* view);

/* Stages the scroll layers for the camera of `game`. */
sat_result_t hsp_view_follow(hsp_view_t* view, const hsp_game_t* game);

/* Queues the sprites and the HUD of one frame. */
sat_result_t hsp_view_draw(hsp_view_t* view, const hsp_game_t* game);

#ifdef __cplusplus
}
#endif

#endif /* HSP_VIEW_MODULE_H */
