/* movie_player - plays the LibSaturn intro (assets/movies/libsaturn_intro.mov) with sat_movie_play, then
 * waits: START plays it again.
 *
 * The movie sits on the disc as LSINTRO.MOV (the Makefile.inc stages it). tools/satmovie.py makes such files
 * from any video: Cinepak pictures and IMA ADPCM sound, read from the disc once through one CD Block play
 * and decoded by the SH-2. A movie smaller than the screen is centred in black.
 *
 * The page after the movie shows what the play counted: frames decoded and skipped, fields in which a frame was
 * due and its data was not (starved), the fewest sectors the ring held, and the longest decode-and-upload.
 * On a real drive these are the numbers that matter (docs/HARDWARE_ACCEPTANCE_CHECKLIST.md). */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/cd_mount.h"
#include "saturn/hud.h"
#include "saturn/movie.h"
#include "saturn/vdp2_color_offset.h"
#include "example_util.h"

static sat_cd_block_t g_block;
static sat_cd_device_t g_device;
static sat_cdfs_volume_t g_volume;
static sat_movie_player_t g_player;
static sat_ascii_font_t g_font;
static sat_hud_t g_hud;

int main(void) {
    sat_movie_stats_t stats = {0};
    uint8_t skipped = 0u;
    sat_result_t status = SAT_OK;

    sat_example_must(sat_app_init_default());
    sat_example_must(sat_cd_mount_default(&g_block, 0, 0, &g_device, 0u, &g_volume));
    sat_example_must(sat_hud_init_default(&g_hud, &g_font));
    for (;;) {
        const sat_movie_play_options_t options = {SAT_PAD_START, 10u, 20u, 0u, {0u, 0u, 0u}};
        status = sat_movie_play(&g_player, &g_volume, &g_block, "LSINTRO.MOV", &options, &skipped, &stats);
        sat_example_must(sat_vdp2_color_offset_disable(SAT_VDP2_LAYER_ALL));
        for (;;) {
            sat_pad_state_t pad = {0};
            sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));
            sat_hud_reset_cursor(&g_hud, 8, 8);
            (void)sat_hud_line(&g_hud, status == SAT_OK ? "PLAYED" : "FAILED", (uint32_t)(status < 0 ? -status : 0));
            (void)sat_hud_line(&g_hud, "SKIPPED BY PAD", skipped);
            (void)sat_hud_line(&g_hud, "FRAMES", stats.frames_shown);
            (void)sat_hud_line(&g_hud, "FRAMES DROPPED", stats.frames_skipped);
            (void)sat_hud_line(&g_hud, "STARVED FIELDS", stats.starved_fields);
            (void)sat_hud_line(&g_hud, "RING LOW SECTORS", stats.ring_low_sectors);
            (void)sat_hud_line(&g_hud, "WORK MAX US", stats.work_max_us);
            (void)sat_hud_text(&g_hud, "START PLAYS IT AGAIN", 8, 100);
            sat_example_must(sat_app_frame_end());
            if ((pad.pressed & SAT_PAD_START) != 0u) break;
        }
    }
}
