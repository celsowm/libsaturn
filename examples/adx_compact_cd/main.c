/* adx_compact_cd: compact ADX CD streaming without linking sat_music.cpp.
 * Caller-owned 64 KiB PCM rings and ~8 KiB decoder staging in Work RAM Low.
 * Uses native SCSP PCM16 stereo, ADX decoded on SH-2; SFX remains resident.
 * A = effect; C = pause/resume; L+R+START = exit.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/adx.h"
#include "saturn/audio.h"
#include "saturn/cd_block.h"
#include "saturn/cd_mount.h"
#include "saturn/cdfs.h"
#include "saturn/color.h"
#include "saturn/disc_stream.h"
#include "saturn/file.h"
#include "saturn/font.h"
#include "saturn/hud.h"
#include "saturn/time.h"
#include "example_util.h"
#include "adx_compact_cd/audio_adx_compact_cd.h"

#define MUSIC_LOGICAL "music/adx"
#define MUSIC_DISC "audio/music.adxbin"
#define MUSIC_SOURCE "disc/audio/music.adxbin"
#define EFFECT_RATE 22050u
#define EFFECT_FRAMES 8000u

static sat_cd_block_t g_cd_block;
static sat_disc_stream_t g_disc __attribute__((section(".wram_l"), aligned(4)));
static sat_adx_pcm_rings_t g_rings __attribute__((section(".wram_l"), aligned(4)));
static sat_adx_player_t g_player __attribute__((section(".wram_l"), aligned(4)));
static sat_cd_device_t g_cd_device;
static sat_cdfs_volume_t g_volume;
static sat_cdfs_file_source_t g_source;
static sat_cdfs_source_desc_t g_source_desc;
static sat_hud_t g_hud;
static int16_t g_effect[EFFECT_FRAMES];
static uint32_t g_worst_hook_us;

static uint32_t ticks_to_us(uint32_t ticks) {
    const uint32_t hz = sat_time_frequency_hz();
    return hz < 1000u ? 0u : (uint32_t)(((uint64_t)ticks * 1000u) / (hz / 1000u));
}

/* Audio serviced from inside a CD Block wait. */
static sat_result_t service_cd_audio(void* context) {
    (void)context;
    const uint64_t t0 = sat_time_ticks();
    const sat_result_t r = sat_audio_update();
    const uint32_t us = ticks_to_us((uint32_t)(sat_time_ticks() - t0));
    if (us > g_worst_hook_us) g_worst_hook_us = us;
    return r;
}

/* A decaying noise burst: an explosion-ish effect with no asset behind it. */
static void make_effect(void) {
    uint32_t seed = 12345u;
    for (uint32_t i = 0u; i < EFFECT_FRAMES; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const int32_t noise = (int32_t)(seed >> 17) - 16384;
        g_effect[i] = (int16_t)((noise * (int32_t)(EFFECT_FRAMES - i)) / (int32_t)EFFECT_FRAMES);
    }
}

static sat_result_t register_music(void) {
    SAT_TRY(sat_cd_mount_default(&g_cd_block, service_cd_audio, 0, &g_cd_device, 0u, &g_volume));
    SAT_TRY(sat_disc_stream_init(&g_disc, &g_cd_block));
    g_source_desc = (sat_cdfs_source_desc_t){
        MUSIC_DISC, MUSIC_SOURCE, SAT_AUDIO_ADX_COMPACT_CD_BYTES};
    SAT_TRY(sat_cdfs_register_source_manifest(&g_volume, &g_source_desc, 1u, &g_source));
    const sat_adx_config_t config = {
        SAT_AUDIO_ADX_COMPACT_CD_SAMPLE_RATE,
        SAT_AUDIO_ADX_COMPACT_CD_SAMPLE_COUNT,
        SAT_AUDIO_ADX_COMPACT_CD_COEFF_1,
        SAT_AUDIO_ADX_COMPACT_CD_COEFF_2,
        SAT_AUDIO_ADX_COMPACT_CD_CHANNELS,
        {0, 0, 0}
    };
    return sat_adx_player_open(&g_player, &g_disc, &g_source.file, &config, &g_rings);
}

int main(void) {
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_audio_init());
    sat_ascii_font_t font;
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u));
    sat_example_must(sat_hud_init(&g_hud, &font, SAT_COLOR_WHITE, 8u));

    make_effect();
    sat_sound_desc_t desc = {0};
    desc.samples = g_effect;
    desc.sample_count = EFFECT_FRAMES;
    desc.sample_rate = EFFECT_RATE;
    desc.format = SAT_AUDIO_PCM_S16;
    sat_sound_t effect;
    sat_example_must(sat_sound_create(&effect, &desc));

    sat_example_must(register_music());
    sat_example_must(sat_music_open(&g_music, MUSIC_LOGICAL));
    sat_example_must(sat_adx_player_play(&g_player));

    uint8_t playing = 1u;
    uint32_t worst_update_us = 0u;
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_GREEN, &pad) != SAT_OK) break;
        if ((pad.held & (SAT_PAD_L | SAT_PAD_R | SAT_PAD_START)) == (SAT_PAD_L | SAT_PAD_R | SAT_PAD_START)) break;
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            sat_sound_play_params_t p;
            p.volume = 255u;
            p.pan = 0;
            p.priority = 10u;
            p.flags = 0u;
            p.pitch = SAT_FX16_ONE;
            (void)sat_sound_play(effect, &p, 0);
        }
        if ((pad.pressed & SAT_PAD_C) != 0u) {
            sat_example_must(playing != 0u ? sat_adx_player_pause(&g_player) : sat_adx_player_play(&g_player));
            playing = (uint8_t)!playing;
        }
        if (playing != 0u) {
            const uint64_t t0 = sat_time_ticks();
            sat_example_must(sat_adx_player_update(&g_player));
            const uint32_t us = ticks_to_us((uint32_t)(sat_time_ticks() - t0));
            if (us > worst_update_us) worst_update_us = us;
        }
        sat_example_must(sat_audio_update());
        if (g_cd_block.progress_error_count != 0u) sat_example_must(g_cd_block.progress_last_error);


        sat_adx_player_info_t info;
        sat_example_must(sat_adx_player_info(&g_player, &info));
        (void)sat_hud_text(&g_hud, "COMPACT ADX FROM CD", 8, 8);
        (void)sat_hud_text(&g_hud, "A EFFECT  C PAUSE", 8, 28);
        (void)sat_hud_value(&g_hud, "STREAM BUF ", info.buffered_frames, 8, 52);
        (void)sat_hud_value(&g_hud, "DECODED FRAMES ", info.decoded_frames, 8, 64);
        (void)sat_hud_value(&g_hud, "STREAM UNDERRUN ", info.underrun_count, 8, 76);
        (void)sat_hud_value(&g_hud, "MAX HOOK US ", g_worst_hook_us, 8, 100);
        (void)sat_hud_value(&g_hud, "MAX POLL US ", ticks_to_us(g_disc.stats.max_ticks_poll), 8, 112);
        (void)sat_hud_value(&g_hud, "MAX DELIVER US ", ticks_to_us(g_disc.stats.max_ticks_music), 8, 124);
        (void)sat_hud_value(&g_hud, "MIN BUFFERED ", g_disc.stats.music_min_buffered, 8, 136);
        (void)sat_hud_value(&g_hud, "MAX UPDATE US ", worst_update_us, 8, 148);
        (void)sat_hud_text(&g_hud, playing == 0u ? "PAUSED" : (info.starting != 0u ? "FILLING RINGS" : "PLAYING FROM ISO"), 8, 172);
        (void)sat_app_frame_end();
    }
    (void)sat_adx_player_close(&g_player);
    (void)sat_cdfs_unmount(&g_volume);
    (void)sat_audio_shutdown();
    (void)sat_shutdown();
    return 0;
}
