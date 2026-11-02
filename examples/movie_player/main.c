/* movie_player - plays the LibSaturn intro (assets/movies/libsaturn_intro.mov) with sat_movie_play, then
 * shows what the play counted. It doubles as a hardware diagnostic: the same VDP2 path the movie uses (a
 * 512x256 15-bit bitmap on NBG0, filled by the CPU or by SCU DMA, flipped between the two VRAM banks) can be
 * run with a fixed picture, so a bad picture on a real console can be told from a bad movie.
 *
 * A moves to the next page, START runs the page again (the movie plays again), B hides the picture of
 * pages 1-3 so the counters under it can be read:
 *   0 MOVIE      the movie, then its result: error code, frames, starved fields, ring, decode time
 *   1 CPU        colour bars, grid and a grey ramp, written by the CPU straight into the displayed bank
 *   2 DMA        the same picture, built in work RAM and moved to VRAM with sat_dma_copy
 *   3 FLIP       a moving bar, uploaded by DMA to the hidden bank and flipped every field, as the movie does
 *   4 INFO       video mode, SCU DMA counters, field count
 * Every page prints the DMA path and the counters, so a photo of any page says how it was made.
 *
 * The movie sits on the disc as LSINTRO.MOV (the Makefile.inc stages it). tools/satmovie.py makes such files
 * from any video: Cinepak pictures and IMA ADPCM sound, read from the disc once through one CD Block play
 * and decoded by the SH-2. A movie smaller than the screen is centred in black.
 *
 * On a real drive the movie numbers are the ones that matter (docs/HARDWARE_ACCEPTANCE_CHECKLIST.md). */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/cd_mount.h"
#include "saturn/dma.h"
#include "saturn/hud.h"
#include "saturn/movie.h"
#include "saturn/vdp2_color_offset.h"
#include "example_util.h"

enum { PAGE_MOVIE, PAGE_CPU, PAGE_DMA, PAGE_FLIP, PAGE_INFO, PAGE_COUNT };

#define VRAM_BASE 0x25E00000u
#define BANK_BYTES 0x40000u
#define SHOWN_W 320u
#define SHOWN_H 224u
#define STRIDE SAT_MOVIE_PICTURE_STRIDE
#define NO_BAR 0xFFFFFFFFu

static sat_cd_block_t g_block;
static sat_cd_device_t g_device;
static sat_cdfs_volume_t g_volume;
static sat_movie_player_t g_player;
static sat_ascii_font_t g_font;
static sat_hud_t g_hud;

static const char* const kPageName[PAGE_COUNT] = {"MOVIE", "CPU PATTERN", "DMA PATTERN", "DMA FLIP", "INFO"};
static const char* const kPathName[4] = {"NONE", "SCU", "CPU", "SH2"};

static uint16_t rgb15(uint32_t r, uint32_t g, uint32_t b) { return (uint16_t)(0x8000u | (b << 10) | (g << 5) | r); }

/* The fixed picture: eight colour bars over a grid over a grey ramp. `bar` is the x of a white bar (FLIP). */
static uint16_t pattern_dot(uint32_t x, uint32_t y, uint32_t bar) {
    static const uint16_t bars[8] = {0x7FFF, 0x7FE0, 0x03FF, 0x03E0, 0x7C1F, 0x001F, 0x7C00, 0x0000};
    if (bar != NO_BAR && x >= bar && x < bar + 8u) return rgb15(31u, 31u, 31u);
    if (y < 96u) return (uint16_t)(0x8000u | bars[x / 40u]);
    if (y < 160u) return ((x % 16u) == 0u || (y % 16u) == 0u) ? rgb15(31u, 31u, 31u) : rgb15(2u, 2u, 6u);
    const uint32_t grey = x * 31u / (SHOWN_W - 1u);
    return rgb15(grey, grey, grey);
}

static void fill_picture(uint16_t* out, uint32_t bar) {
    for (uint32_t y = 0u; y < SHOWN_H; ++y) {
        for (uint32_t x = 0u; x < SHOWN_W; ++x) out[y * STRIDE + x] = pattern_dot(x, y, bar);
    }
}

static void fill_vram_by_cpu(uint32_t bank_offset) {
    volatile uint16_t* out = (volatile uint16_t*)(VRAM_BASE + bank_offset);
    for (uint32_t y = 0u; y < SHOWN_H; ++y) {
        for (uint32_t x = 0u; x < SHOWN_W; ++x) out[y * STRIDE + x] = pattern_dot(x, y, NO_BAR);
    }
}

static sat_result_t bitmap_layer_on(void) {
    sat_vdp2_layer_config_t layer;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &layer);
    layer.bitmap = 1u;
    layer.bitmap_size = SAT_VDP2_BITMAP_512X256;
    layer.color_mode = SAT_VDP2_COLOR_MODE_32768;
    layer.priority = 7u;
    layer.transparent = 0u;
    for (unsigned i = 0u; i < 4u; ++i) layer.plane_address[i] = 0u;
    SAT_TRY(sat_vdp2_layer_configure(&layer));
    SAT_TRY(sat_vdp2_layer_set_pattern_name_base(SAT_VDP2_NBG0, 0u));
    return sat_vdp2_layers_commit();
}

static void print_counters(const char* page, sat_result_t status) {
    sat_dma_stats_t dma;
    sat_dma_get_stats(&dma);
    sat_hud_reset_cursor(&g_hud, 8, 8);
    (void)sat_hud_line(&g_hud, page, 0u);
    (void)sat_hud_line(&g_hud, "RESULT", (uint32_t)(status < 0 ? -status : 0));
    (void)sat_hud_line(&g_hud, "VIDEO W", sat_video_width());
    (void)sat_hud_line(&g_hud, "VIDEO H", sat_video_height());
    (void)sat_hud_line(&g_hud, "SCU DMA", dma.scu_transfers);
    (void)sat_hud_line(&g_hud, "CPU COPIES", dma.cpu_copies);
    (void)sat_hud_line(&g_hud, "DMA TIMEOUTS", dma.timeouts);
    (void)sat_hud_line(&g_hud, "DMA ILLEGAL", dma.illegal);
    (void)sat_hud_line(&g_hud, kPathName[(uint32_t)dma.last_path & 3u], 0u);
}

int main(void) {
    sat_movie_stats_t stats = {0};
    uint8_t skipped = 0u;
    sat_result_t status = SAT_OK;
    uint32_t page = PAGE_MOVIE;

    sat_example_must(sat_app_init_default());
    sat_example_must(sat_cd_mount_default(&g_block, 0, 0, &g_device, 0u, &g_volume));
    sat_example_must(sat_hud_init_default(&g_hud, &g_font));
    for (;;) {
        uint32_t shown_base = 0u, bar = 0u;
        uint8_t picture_on = 1u;
        if (page == PAGE_MOVIE) {
            const sat_movie_play_options_t options = {SAT_PAD_START, 10u, 20u, 0u, {0u, 0u, 0u}};
            status = sat_movie_play(&g_player, &g_volume, &g_block, "LSINTRO.MOV", &options, &skipped, &stats);
        } else if (page == PAGE_INFO) {
            status = sat_vdp2_layer_set_enabled(SAT_VDP2_NBG0, 0u);
            if (status == SAT_OK) status = sat_vdp2_layers_commit();
        } else {
            status = bitmap_layer_on();
            if (page == PAGE_CPU) {
                fill_vram_by_cpu(0u);
            } else {
                fill_picture(g_player.picture, NO_BAR);
                if (status == SAT_OK) {
                    status = sat_dma_copy((void*)VRAM_BASE, g_player.picture, sizeof g_player.picture);
                }
            }
        }
        sat_example_must(sat_vdp2_color_offset_disable(SAT_VDP2_LAYER_ALL));
        for (;;) {
            sat_pad_state_t pad = {0};
            sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));
            if (page == PAGE_FLIP && status == SAT_OK) {
                const uint32_t hidden = shown_base != 0u ? 0u : BANK_BYTES;
                bar = (bar + 4u) % (SHOWN_W - 8u);
                fill_picture(g_player.picture, bar);
                status = sat_dma_copy((void*)(VRAM_BASE + hidden), g_player.picture, sizeof g_player.picture);
                if (status == SAT_OK) status = sat_vdp2_layer_set_pattern_name_base(SAT_VDP2_NBG0, hidden);
                shown_base = hidden;
                (void)sat_vdp2_layers_commit();
            }
            print_counters(kPageName[page], status);
            if (page == PAGE_MOVIE) {
                (void)sat_hud_line(&g_hud, "SKIPPED BY PAD", skipped);
                (void)sat_hud_line(&g_hud, "FRAMES", stats.frames_shown);
                (void)sat_hud_line(&g_hud, "FRAMES DROPPED", stats.frames_skipped);
                (void)sat_hud_line(&g_hud, "STARVED FIELDS", stats.starved_fields);
                (void)sat_hud_line(&g_hud, "RING LOW SECTORS", stats.ring_low_sectors);
                (void)sat_hud_line(&g_hud, "WORK MAX US", stats.work_max_us);
            }
            if (page == PAGE_INFO) (void)sat_hud_line(&g_hud, "FIELDS", sat_field_count());
            (void)sat_hud_text(&g_hud, "A NEXT  B TEXT  START AGAIN", 8, 200);
            sat_example_must(sat_app_frame_end());
            if ((pad.pressed & SAT_PAD_B) != 0u && page != PAGE_MOVIE && page != PAGE_INFO) {
                picture_on = (uint8_t)!picture_on;                    /* the text sits under the picture */
                (void)sat_vdp2_layer_set_enabled(SAT_VDP2_NBG0, picture_on);
                (void)sat_vdp2_layers_commit();
            }
            if ((pad.pressed & SAT_PAD_A) != 0u) {
                page = (page + 1u) % PAGE_COUNT;
                break;
            }
            if ((pad.pressed & SAT_PAD_START) != 0u) break;
        }
    }
}
