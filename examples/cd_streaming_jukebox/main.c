/* cd_streaming_jukebox - CD Block -> CDFS -> VFS -> non-resident music.
 *
 * The executable contains only this control/UI code. The public-domain PCM
 * tracks are staged as ISO files by Makefile.inc, mounted through CDFS, and
 * served to sat_music_update through the bounded asset cache.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/asset.h"
#include "saturn/audio.h"
#include "saturn/cd_block.h"
#include "saturn/cdfs.h"
#include "saturn/color.h"
#include "saturn/file.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/hud.h"
#include "cd_streaming_jukebox/jukebox_data.h"

#define TRACK_COUNT CD_JUKEBOX_TRACK_COUNT
#define PREFETCH_BYTES (SAT_ASSET_CACHE_BLOCK_BYTES * SAT_ASSET_CACHE_DEFAULT_BLOCK_CAPACITY)

typedef struct track {
    const char* title;
    const char* logical_path;
    const char* disc_path;
    const char* source_path;
    uint32_t expected_bytes;
} track_t;

static const track_t g_tracks[TRACK_COUNT] = {
    {"ODE TO JOY - BEETHOVEN", "music/ode-to-joy", "audio/ode_to_joy.pcm",
     "disc/audio/ode_to_joy.pcm", CD_JUKEBOX_ODE_TO_JOY_BYTES},
    {"MINUET IN G - PETZOLD", "music/minuet-in-g", "audio/minuet_in_g.pcm",
     "disc/audio/minuet_in_g.pcm", CD_JUKEBOX_MINUET_IN_G_BYTES},
    {"GREENSLEEVES - TRADITIONAL", "music/greensleeves", "audio/greensleeves.pcm",
     "disc/audio/greensleeves.pcm", CD_JUKEBOX_GREENSLEEVES_BYTES},
};

static sat_cd_block_t g_cd_block;
static sat_cd_device_t g_cd_device;
static sat_cdfs_volume_t g_volume;
static sat_cdfs_file_source_t g_sources[TRACK_COUNT];
static sat_cdfs_source_desc_t g_source_manifest[TRACK_COUNT];
static sat_asset_desc_t g_asset_manifest[TRACK_COUNT];
static sat_asset_t g_asset_handles[TRACK_COUNT];
static sat_music_t g_music;
static sat_hud_t g_hud;
static sat_asset_prefetch_t g_prefetch;
static uint8_t g_prefetch_active;
static uint8_t g_track_index;
static uint8_t g_playing;

static void draw_line(const sat_ascii_font_t* font, const char* text, int y) {
    (void)font;
    (void)sat_hud_text(&g_hud, text, 8, y);
}

static void draw_value(const sat_ascii_font_t* font, const char* label, uint32_t value, int y) {
    (void)font;
    (void)sat_hud_value(&g_hud, label, value, 8, y);
}

static sat_result_t service_cd_audio(void* context) {
    (void)context;
    return sat_audio_update();
}

static sat_result_t register_cd_tracks(void) {
    sat_result_t status = sat_cd_block_init(&g_cd_block, SAT_CD_BLOCK_DEFAULT_TIMEOUT);
    if (status != SAT_OK) return status;
    /* This example explicitly opts into servicing audio while CD waits.
     * Bare-metal CD Block users have no implicit audio dependency. */
    status = sat_cd_block_set_progress_service(&g_cd_block, service_cd_audio, 0);
    if (status != SAT_OK) return status;
    /* A zero sector count means unbounded validation in sat_cd_device_t. The
     * CD Block itself remains bounded by the requested CDFS file extent. */
    status = sat_cd_block_bind_device(&g_cd_block, &g_cd_device, 0u);
    if (status != SAT_OK) return status;
    status = sat_cdfs_mount(&g_volume, &g_cd_device);
    if (status != SAT_OK) return status;

    for (uint16_t i = 0u; i < TRACK_COUNT; ++i) {
        g_source_manifest[i] = (sat_cdfs_source_desc_t){
            g_tracks[i].disc_path, g_tracks[i].source_path, g_tracks[i].expected_bytes};
    }
    status = sat_cdfs_register_source_manifest(
        &g_volume, g_source_manifest, TRACK_COUNT, g_sources);
    if (status != SAT_OK) return status;
    for (uint16_t i = 0u; i < TRACK_COUNT; ++i) {

        g_asset_manifest[i] = (sat_asset_desc_t){0};
        g_asset_manifest[i].logical_path = g_tracks[i].logical_path;
        g_asset_manifest[i].source_path = g_tracks[i].source_path;
        g_asset_manifest[i].size = g_sources[i].file.size;
        g_asset_manifest[i].sample_rate = CD_JUKEBOX_SAMPLE_RATE;
        g_asset_manifest[i].sample_count = g_sources[i].file.size /
            (2u * CD_JUKEBOX_CHANNELS);
        g_asset_manifest[i].channels = CD_JUKEBOX_CHANNELS;
        g_asset_manifest[i].format = SAT_AUDIO_PCM_S16;
        g_asset_manifest[i].kind = SAT_ASSET_STREAM;
    }
    status = sat_asset_register_manifest(
        g_asset_manifest, TRACK_COUNT, g_asset_handles);
    if (status != SAT_OK) return status;
    for (uint16_t i = 0u; i < TRACK_COUNT; ++i) {
        /* Prove each catalog entry resolves through the mounted CDFS source,
         * while keeping its payload non-resident. */
        uint8_t signature[2] = {0};
        uint32_t read = 0u;
        status = sat_asset_read_at(g_tracks[i].logical_path, 0u, signature, sizeof(signature), &read);
        if (status != SAT_OK || read != sizeof(signature)) return SAT_ERR_IO;
    }
    return SAT_OK;
}

static sat_result_t warm_selected_track(void) {
    if (g_prefetch_active != 0u) {
        (void)sat_asset_prefetch_cancel(g_prefetch);
        g_prefetch_active = 0u;
    }
    sat_result_t status = sat_asset_prefetch_submit(
        g_tracks[g_track_index].logical_path, 0u, PREFETCH_BYTES, &g_prefetch);
    if (status == SAT_OK) g_prefetch_active = 1u;
    return status;
}

static sat_result_t open_selected_track(void) {
    if (g_music.generation != 0u) {
        sat_result_t status = sat_music_close(g_music);
        if (status != SAT_OK) return status;
        g_music = (sat_music_t){0};
    }
    sat_result_t status = warm_selected_track();
    if (status != SAT_OK) return status;
    status = sat_music_open(&g_music, g_tracks[g_track_index].logical_path);
    if (status != SAT_OK) return status;
    status = sat_music_play(g_music);
    if (status == SAT_OK) g_playing = 1u;
    return status;
}

static sat_result_t select_track(int8_t delta) {
    int16_t selected = (int16_t)g_track_index + delta;
    if (selected < 0) selected = TRACK_COUNT - 1u;
    if (selected >= (int16_t)TRACK_COUNT) selected = 0u;
    g_track_index = (uint8_t)selected;
    return open_selected_track();
}

int main(void) {
    sat_result_t status = sat_app_init_default();
    if (status != SAT_OK) return 1;
    if (status == SAT_OK) status = sat_audio_init();
    if (status == SAT_OK) status = register_cd_tracks();
    if (status == SAT_OK) status = open_selected_track();

    sat_ascii_font_t font;
    if (status == SAT_OK) {
        status = sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u);
    }
    if (status == SAT_OK) status = sat_hud_init(&g_hud, &font, SAT_COLOR_WHITE, 8u);

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK,
                status == SAT_OK ? SAT_COLOR_GREEN : SAT_COLOR_RED, &pad) != SAT_OK) {
            break;
        }
        if (status == SAT_OK && (pad.pressed & SAT_PAD_X) != 0u) status = select_track(-1);
        if (status == SAT_OK && (pad.pressed & SAT_PAD_Y) != 0u) status = select_track(1);
        if (status == SAT_OK && (pad.pressed & SAT_PAD_C) != 0u) {
            if (g_playing != 0u) {
                status = sat_music_pause(g_music);
                if (status == SAT_OK) g_playing = 0u;
            } else {
                status = sat_music_resume(g_music);
                if (status == SAT_OK) g_playing = 1u;
            }
        }
        if (status == SAT_OK) status = sat_asset_prefetch_update();
        if (status == SAT_OK && g_playing != 0u) status = sat_music_update(g_music);
        if (status == SAT_OK) status = sat_audio_update();

        if (status == SAT_OK) {
            sat_asset_cache_stats_t cache;
            sat_audio_stream_stats_t stream;
            if (sat_asset_cache_stats(&cache) != SAT_OK || sat_music_stats(g_music, &stream) != SAT_OK) {
                status = SAT_ERR_IO;
            } else {
                draw_line(&font, "CD STREAMING JUKEBOX", 8);
                draw_line(&font, g_tracks[g_track_index].title, 28);
                draw_line(&font, "X/Y TRACK  C PLAY/PAUSE", 48);
                draw_line(&font, "CD BLOCK -> CDFS -> VFS", 60);
                draw_value(&font, "CACHE HITS ", cache.hits, 88);
                draw_value(&font, "CACHE MISSES ", cache.misses, 100);
                draw_value(&font, "CACHE FILLS ", cache.fills, 112);
                draw_value(&font, "PREFETCH OK ", cache.prefetch_completed, 124);
                draw_value(&font, "STREAM BUF ", stream.buffered_frames, 148);
                draw_value(&font, "STREAM REFILLS ", stream.refill_count, 160);
                draw_value(&font, "STREAM UNDERRUN ", stream.underrun_count, 172);
                draw_line(&font, g_playing != 0u ? "PLAYING FROM ISO" : "PAUSED", 196);
                draw_line(&font, "PUBLIC DOMAIN MELODIES", 208);
            }
        }
        (void)sat_app_frame_end();
    }

    if (g_music.generation != 0u) (void)sat_music_close(g_music);
    if (g_volume.mounted != 0u) (void)sat_cdfs_unmount(&g_volume);
    (void)sat_audio_shutdown();
    (void)sat_shutdown();
    return status == SAT_OK ? 0 : 1;
}
