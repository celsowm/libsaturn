#include "music_68k.h"

#if DINO_MUSIC_68K

#include <stdint.h>

#include "saturn/audio.h"
#include "saturn/sound_driver.h"

#include "dino_demo/audio_dino_drone.h"
#include "dino_demo/audio_dino_figure.h"
#include "dino_demo/audio_dino_impact.h"
#include "dino_demo/audio_dino_signal.h"

#define BANK_VOICES 4u
#define SCORE_CAPACITY 192u
#define SAMPLE_CAPACITY 33000u
#define DINO_WRAM_L __attribute__((section(".wram_l")))

typedef struct bank_entry {
    const char* path;
    uint32_t size;
    uint32_t sample_count;
    int16_t pan;
} bank_entry_t;

static const bank_entry_t g_bank[BANK_VOICES] = {
    {"audio/dino_drone.s8", SAT_AUDIO_DINO_DRONE_BYTES,
     SAT_AUDIO_DINO_DRONE_SAMPLE_COUNT, -4},
    {"audio/dino_figure.s8", SAT_AUDIO_DINO_FIGURE_BYTES,
     SAT_AUDIO_DINO_FIGURE_SAMPLE_COUNT, 3},
    {"audio/dino_impact.s8", SAT_AUDIO_DINO_IMPACT_BYTES,
     SAT_AUDIO_DINO_IMPACT_SAMPLE_COUNT, -2},
    {"audio/dino_signal.s8", SAT_AUDIO_DINO_SIGNAL_BYTES,
     SAT_AUDIO_DINO_SIGNAL_SAMPLE_COUNT, 6},
};

static uint8_t g_sample[SAMPLE_CAPACITY] DINO_WRAM_L;
static sat_sound_score_event_t g_score[SCORE_CAPACITY] DINO_WRAM_L;
static sat_sound_t g_sounds[BANK_VOICES];
static sat_voice_t g_voices[BANK_VOICES];
static uint16_t g_score_count;

static sat_result_t append(uint32_t milliseconds, uint16_t reg, uint16_t value) {
    if (g_score_count >= SCORE_CAPACITY) return SAT_ERR_CAPACITY;
    sat_sound_score_event_t event = {
        sat_sound_driver_ticks_from_ms(milliseconds), reg, value
    };
    uint16_t position = g_score_count++;
    while (position != 0u && g_score[position - 1u].tick > event.tick) {
        g_score[position] = g_score[position - 1u];
        --position;
    }
    g_score[position] = event;
    return SAT_OK;
}

static uint16_t register_of(uint8_t voice, uint16_t offset) {
    return (uint16_t)(g_voices[voice].slot * 0x20u + offset);
}

static sat_result_t level(uint32_t ms, uint8_t voice, uint8_t attenuation) {
    return append(ms, register_of(voice, 0x0Cu), attenuation);
}

static sat_result_t pitch(uint32_t ms, uint8_t voice, uint16_t value) {
    return append(ms, register_of(voice, 0x10u), value);
}

static sat_result_t load_voice(sat_cdfs_volume_t* volume, uint8_t index) {
    sat_cdfs_file_t file;
    uint32_t read = 0u;
    sat_sound_desc_t desc = {0};
    sat_sound_play_params_t params = {0};
    sat_result_t st = sat_cdfs_lookup(volume, g_bank[index].path, &file);
    if (st != SAT_OK) return st;
    if (file.size != g_bank[index].size || file.size > SAMPLE_CAPACITY ||
        file.size != g_bank[index].sample_count) return SAT_ERR_INVALID_ARG;
    st = sat_cdfs_read_at(volume, &file, 0u, g_sample, file.size, &read);
    if (st != SAT_OK) return st;
    if (read != file.size) return SAT_ERR_IO;
    desc.samples = g_sample;
    desc.sample_count = file.size;
    desc.sample_rate = 11025u;
    desc.format = SAT_AUDIO_PCM_S8;
    desc.loop = 1u;
    st = sat_sound_create(&g_sounds[index], &desc);
    if (st != SAT_OK) return st;
    params.volume = 0u;
    params.pan = g_bank[index].pan;
    params.priority = 10u;
    params.pitch = SAT_FX16_ONE;
    return sat_sound_play(g_sounds[index], &params, &g_voices[index]);
}

/* Sparse 96-second cycle: one low drone, a few hollow figure notes, isolated
 * impacts and rare high signals. Every change is executed by the 68000 on its
 * timer, independent of rendering and disc reads. TL=255 is silence. */
static sat_result_t compose(void) {
    static const uint32_t figure_ms[] = {9000u, 15600u, 21800u, 33700u,
        40700u, 51200u, 62200u, 71300u, 84300u};
    static const uint16_t figure_pitch[] = {0x7000u, 0x707Du, 0x703Du,
        0x7000u, 0x710Au, 0x707Du, 0x703Du, 0x7000u, 0x707Du};
    static const uint32_t impact_ms[] = {26400u, 46200u, 68800u, 88300u};
    static const uint32_t signal_ms[] = {36400u, 58200u, 78600u};
    g_score_count = 0u;
    if (level(0u, 0u, 28u) != SAT_OK) return SAT_ERR_CAPACITY;
    for (uint32_t ms = 6000u; ms < 96000u; ms += 12000u) {
        if (level(ms, 0u, 34u) != SAT_OK) return SAT_ERR_CAPACITY;
        if (ms + 6000u < 96000u &&
            level(ms + 6000u, 0u, 27u) != SAT_OK) return SAT_ERR_CAPACITY;
    }
    for (uint32_t i = 0u; i < sizeof(figure_ms) / sizeof(figure_ms[0]); ++i) {
        const uint32_t ms = figure_ms[i];
        if (pitch(ms, 1u, figure_pitch[i]) != SAT_OK ||
            level(ms, 1u, 38u) != SAT_OK ||
            level(ms + 2400u, 1u, 255u) != SAT_OK) return SAT_ERR_CAPACITY;
    }
    for (uint32_t i = 0u; i < sizeof(impact_ms) / sizeof(impact_ms[0]); ++i) {
        const uint32_t ms = impact_ms[i];
        if (pitch(ms, 2u, 0x6800u) != SAT_OK ||
            level(ms, 2u, 28u) != SAT_OK ||
            level(ms + 380u, 2u, 46u) != SAT_OK ||
            level(ms + 1150u, 2u, 255u) != SAT_OK) return SAT_ERR_CAPACITY;
    }
    for (uint32_t i = 0u; i < sizeof(signal_ms) / sizeof(signal_ms[0]); ++i) {
        const uint32_t ms = signal_ms[i];
        if (pitch(ms, 3u, 0x7800u) != SAT_OK ||
            level(ms, 3u, 48u) != SAT_OK ||
            level(ms + 600u, 3u, 255u) != SAT_OK) return SAT_ERR_CAPACITY;
    }
    /* The last drone fade approaches the beginning without a hard level jump. */
    if (level(94000u, 0u, 30u) != SAT_OK) return SAT_ERR_CAPACITY;
    return sat_sound_driver_score_load(
        g_score, g_score_count, sat_sound_driver_ticks_from_ms(96000u));
}

sat_result_t dino_music_68k_start(sat_cdfs_volume_t* volume) {
    if (volume == 0) return SAT_ERR_INVALID_ARG;
    sat_result_t st = sat_sound_driver_start();
    if (st != SAT_OK) return st;
    for (uint8_t i = 0u; i < BANK_VOICES; ++i) {
        st = load_voice(volume, i);
        if (st != SAT_OK) return st;
    }
    return compose();
}

#endif
