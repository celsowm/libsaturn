#ifndef LIBSATURN_EXAMPLES_CHARACTERS_STAGE_H
#define LIBSATURN_EXAMPLES_CHARACTERS_STAGE_H

#include <stdint.h>

#include "saturn/core.h"

/* The VDP2 fighting stage used by vdp2_fighting_stage (E11). It composes four
 * NBGs behind the existing 3D fighter renderer: stands with a moving crowd,
 * banners, moving spotlights and a ring apron. The fighter simulation,
 * meshes, painter and VDP1 command path are the normal character demo; this
 * module only owns the VDP2 backdrop. Hiding the backdrop (L in the example,
 * automatically around frames 70-120) lets the same fighter frame be
 * compared with and without it.
 *
 * The implementation lives in examples/characters/stage.c and is compiled
 * only for the E11 example. It uses the public VDP2 layer, window and colour
 * offset APIs, never raw register pokes. */

typedef struct vdp2_stage_telemetry {
    uint32_t magic;
    uint32_t init_ok;
    uint32_t layers_ok;
    uint32_t window_ok;
    uint32_t color_offset_ok;
    uint32_t refused_window;
    uint32_t refused_scroll;
    uint32_t frames;
    uint32_t stage_on;
    uint32_t toggles;
    uint32_t auto_toggles;
    uint32_t stage_visible_frames;
    uint32_t commands_on;
    uint32_t commands_off;
    uint32_t geometry_us_on;
    uint32_t geometry_us_off;
    uint32_t last_commands;
    uint32_t last_geometry_us;
} vdp2_stage_telemetry_t;

extern volatile vdp2_stage_telemetry_t g_vdp2_stage;

/* Configures and shows the four stage layers. Returns SAT_ERR_CAPACITY when
 * the layer manager cannot fetch all of them; the telemetry records which
 * step failed. */
sat_result_t vdp2_stage_init(void);

/* Per-field update: scroll, colour offset and the VBlank layer commit. The
 * caller has waited for VBlank. Also runs the deterministic on/off
 * demonstration at frames 70 and 120. */
void vdp2_stage_frame(void);

/* Shows or hides the whole backdrop without touching the fighter renderer. */
void vdp2_stage_toggle(void);
uint32_t vdp2_stage_on(void);

/* Called by the demo after it reads the frame's VDP1 command count and
 * geometry preparation time, so both stage states are measured. */
void vdp2_stage_note_frame(uint32_t commands, uint32_t geometry_us);

#endif /* LIBSATURN_EXAMPLES_CHARACTERS_STAGE_H */
