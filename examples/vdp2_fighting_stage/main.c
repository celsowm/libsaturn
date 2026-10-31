/* VDP2 fighting stage (E11).
 *
 * The existing two-fighter character demo (same assets, combat simulation,
 * skeleton evaluation, global painter and VDP1 command path as
 * fighter_arena_3d) with a VDP2 stage behind it instead of the legacy NBG0
 * backdrop:
 *
 *   stands and a drifting crowd  NBG3, priority 1
 *   hanging banners              NBG2, priority 3, animated colour offset
 *   two moving spotlights        NBG1, priority 2, window W0 clips the hall
 *   ring apron rails             NBG0, priority 4
 *
 * The training mat, fighters, shadows and HUD are still the normal scene
 * painter, so the stage cannot cover a fighter (sprite priority 6). L hides
 * and shows the backdrop; the example also does it deterministically at
 * frames 70 and 120 for the harness. The fighter simulation is identical in
 * both states, which is what the acceptance compares.
 *
 * g_vdp2_stage (stage telemetry) and g_character_demo (fighter metrics) hold
 * what harness/tests/test_vdp2_fighting_stage.py checks. */
#include "examples/characters/demo.h"

int main(void) { return character_demo_run(CHARACTER_DEMO_STAGE); }
