/* cd_streaming_jukebox - CD Block -> CDFS -> VFS -> non-resident music.
 *
 * The executable contains only this control/UI code. The public-domain PCM
 * tracks are staged as ISO files by Makefile.inc and mounted through CDFS.
 *
 * By default the music streams through the library disc scheduler
 * (JUKEBOX_DISC_STREAM=1, saturn/disc_stream.h): sat_music_play fills the rings with
 * blocking reads, then sat_music_update only lands the 16 KiB segment that has
 * arrived and starts the next, so no frame waits for the drive. With
 * JUKEBOX_DISC_STREAM=0 the tracks are served through the bounded asset
 * cache instead and a refill stalls the frame for the 64 KiB block read.
 * The HUD shows the worst frame time either way.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/asset.h"
#include "saturn/audio.h"
#include "saturn/cd_block.h"
#include "saturn/cd_mount.h"
#include "saturn/cdfs.h"
#include "saturn/disc_stream.h"
#include "saturn/time.h"
#include "saturn/color.h"
#include "saturn/file.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/hud.h"
#include "cd_streaming_jukebox/jukebox_data.h"

#ifndef JUKEBOX_DISC_STREAM
#define JUKEBOX_DISC_STREAM 1
#endif

/* 1: the audio driver rewrites an inactive SCSP half over several frames (sat_audio_set_refill_spread)
 * instead of in the one update that sees the change. */
#ifndef JUKEBOX_REFILL_SPREAD
#define JUKEBOX_REFILL_SPREAD 1
#endif

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
#if JUKEBOX_DISC_STREAM
static sat_disc_stream_t g_disc;
#endif
static uint32_t g_worst_frame_ms;
static uint32_t g_frames_seen;
static uint32_t g_worst_update_us;     /* the slowest sat_music_update call */
static uint32_t g_worst_hook_us;       /* the slowest audio service run from inside a CD Block wait */
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

static uint32_t ticks_to_us(uint32_t ticks) {
    const uint32_t hz = sat_time_frequency_hz();
    return hz < 1000u ? 0u : (uint32_t)(((uint64_t)ticks * 1000u) / (hz / 1000u));
}

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
    const uint64_t t0 = sat_time_ticks();
    const sat_result_t r = sat_audio_update();
    const uint32_t us = ticks_to_us((uint32_t)(sat_time_ticks() - t0));
    if (us > g_worst_hook_us) g_worst_hook_us = us;
    return r;
}

static sat_result_t register_cd_tracks(void) {
    /* A zero sector count means unbounded validation in sat_cd_device_t. The
     * CD Block itself remains bounded by the requested CDFS file extent.
     * service_cd_audio is the opt-in: bare-metal CD Block users have no
     * implicit audio dependency. */
    sat_result_t status = sat_cd_mount_default(
        &g_cd_block, service_cd_audio, 0, &g_cd_device, 0u, &g_volume);
    if (status != SAT_OK) return status;
#if JUKEBOX_DISC_STREAM
    status = sat_disc_stream_init(&g_disc, &g_cd_block);
    if (status == SAT_OK) status = sat_music_use_disc_stream(&g_disc);
    if (status != SAT_OK) return status;
#endif

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
#if JUKEBOX_DISC_STREAM
    /* The scheduler reads the track itself; a cache prefetch would take the drive from it. */
    return SAT_OK;
#endif
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
#if JUKEBOX_REFILL_SPREAD
    if (status == SAT_OK) status = sat_audio_set_refill_spread(1u);
#endif
    if (status == SAT_OK) status = register_cd_tracks();
    if (status == SAT_OK) status = open_selected_track();

    sat_ascii_font_t font;
    if (status == SAT_OK) {
        status = sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u);
    }
    if (status == SAT_OK) status = sat_hud_init(&g_hud, &font, SAT_COLOR_WHITE, 8u);

    uint32_t last_ms = sat_time_ms();
    for (;;) {
        sat_pad_state_t pad = {0};
        {
            const uint32_t now = sat_time_ms();
            if (now - last_ms > g_worst_frame_ms && g_frames_seen > 2u) g_worst_frame_ms = now - last_ms;
            last_ms = now;
            ++g_frames_seen;
        }
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
        if (status == SAT_OK && (pad.pressed & SAT_PAD_B) != 0u) {
            sat_music_info_t info;
            status = sat_music_info(g_music, &info);
            if (status == SAT_OK) status = sat_music_seek(g_music, info.sample_count / 2u);
        }
#if !JUKEBOX_DISC_STREAM
        if (status == SAT_OK) status = sat_asset_prefetch_update();
#endif
        if (status == SAT_OK && g_playing != 0u) {
            const uint64_t t0 = sat_time_ticks();
            status = sat_music_update(g_music);
            const uint32_t us = ticks_to_us((uint32_t)(sat_time_ticks() - t0));
            if (us > g_worst_update_us) g_worst_update_us = us;
        }
        if (status == SAT_OK) status = sat_audio_update();
        /* Audio serviced from inside a CD wait must not fail silently. */
        if (status == SAT_OK && g_cd_block.progress_error_count != 0u) {
            status = g_cd_block.progress_last_error;
        }

        if (status == SAT_OK) {
            sat_asset_cache_stats_t cache;
            sat_audio_stream_stats_t stream;
            if (sat_asset_cache_stats(&cache) != SAT_OK || sat_music_stats(g_music, &stream) != SAT_OK) {
                status = SAT_ERR_IO;
            } else {
                draw_line(&font, "CD STREAMING JUKEBOX", 8);
                draw_line(&font, g_tracks[g_track_index].title, 28);
                draw_line(&font, "X/Y TRACK C PAUSE B SEEK", 48);
                draw_line(&font, "CD BLOCK -> CDFS -> VFS", 60);
#if JUKEBOX_DISC_STREAM
                /* The slowest single poll and music delivery. */
                draw_value(&font, "MAX HOOK US ", g_worst_hook_us, 88);
                draw_value(&font, "MAX POLL US ", ticks_to_us(g_disc.stats.max_ticks_poll), 100);
                draw_value(&font, "MAX DELIVER US ", ticks_to_us(g_disc.stats.max_ticks_music), 112);
                draw_value(&font, "MIN BUFFERED ", g_disc.stats.music_min_buffered, 124);
#else
                draw_value(&font, "CACHE HITS ", cache.hits, 88);
                draw_value(&font, "CACHE MISSES ", cache.misses, 100);
                draw_value(&font, "CACHE FILLS ", cache.fills, 112);
                draw_value(&font, "PREFETCH OK ", cache.prefetch_completed, 124);
#endif
                draw_value(&font, "STREAM BUF ", stream.buffered_frames, 136);
                draw_value(&font, "STREAM REFILLS ", stream.refill_count, 148);
                draw_value(&font, "STREAM UNDERRUN ", stream.underrun_count, 160);
                draw_value(&font, "WORST FRAME MS ", g_worst_frame_ms, 172);
                draw_value(&font, "WORST UPDATE US ", g_worst_update_us, 184);
                draw_line(&font, g_playing != 0u ? "PLAYING FROM ISO" : "PAUSED", 196);
                draw_line(&font, JUKEBOX_DISC_STREAM ? "DISC SCHEDULER" : "ASSET CACHE", 208);
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
