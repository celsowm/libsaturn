/* The resident 68000 sound driver: timestamped SCSP writes run by the sound CPU.
 * The SH-2 schedules eight markers 20 ticks apart, a timed key-on of a looping
 * tone and a timed key-off, and reads back when each ran.
 * g_driver_demo holds what harness/tests/test_sound_driver.py checks. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/audio.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/sound_driver.h"
#include "saturn/video.h"

#define DRIVER_DEMO_MAGIC 0x53445231u /* "SDR1" */
#define MARKERS 8u
#define MARKER_SPACING 20u
#define TONE_SAMPLES 441u

typedef struct driver_demo_results {
    uint32_t magic;
    uint32_t audio_status;
    uint32_t start_status;
    uint32_t running;
    uint32_t base_tick;                 /* tick the schedule started from */
    uint32_t marker_status;             /* OR of the schedule results, as -result */
    uint32_t play_status;               /* sat_sound_play with a start tick */
    uint32_t play_start_tick;
    uint32_t key_off_status;
    uint32_t voice_a_slot;              /* keyed on and left on */
    uint32_t voice_b_slot;              /* keyed on, then off again */
    uint32_t tick_a;                    /* driver tick when frame_a was drawn */
    uint32_t frame_a;
    uint32_t tick_b;
    uint32_t frame_b;
    uint32_t heartbeat;
    uint32_t executed;
    uint32_t max_lateness;
    uint32_t queued_at_end;
    uint32_t log_ticks[MARKERS + 3u];   /* the tick each scheduled event ran at */
    uint32_t log_late[MARKERS + 3u];
    uint32_t frames;
} driver_demo_results_t;

volatile driver_demo_results_t g_driver_demo;

static sat_ascii_font_t font;
static int16_t g_tone[TONE_SAMPLES];

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    sat_sound_t tone = {0};
    sat_sound_desc_t desc = {0};
    sat_sound_play_params_t params = {0};
    sat_voice_t voice = {0};
    sat_voice_t voice_a = {0};
    sat_sound_driver_info_t info = {0};
    uint32_t base;
    uint32_t results = 0u;

    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_driver_demo.magic = DRIVER_DEMO_MAGIC;

    /* a square tone, 100 Hz at 44.1 kHz per period of 441 samples */
    for (uint32_t i = 0u; i < TONE_SAMPLES; ++i) g_tone[i] = (int16_t)(i < TONE_SAMPLES / 2u ? 6000 : -6000);
    desc.samples = g_tone;
    desc.sample_count = TONE_SAMPLES;
    desc.sample_rate = 44100u;
    desc.format = SAT_AUDIO_PCM_S16;
    desc.loop = 1u;

    g_driver_demo.audio_status = (uint32_t)(-sat_audio_init());
    if (g_driver_demo.audio_status == 0u) {
        g_driver_demo.start_status = (uint32_t)(-sat_sound_driver_start());
        g_driver_demo.running = sat_sound_driver_running();
    }
    if (g_driver_demo.running != 0u && sat_sound_create(&tone, &desc) == SAT_OK) {
        base = sat_sound_driver_tick() + 10u;
        g_driver_demo.base_tick = base;
        for (uint32_t i = 1u; i <= MARKERS; ++i) {
            results |= (uint32_t)(-sat_sound_driver_schedule_marker(base + i * MARKER_SPACING));
        }
        g_driver_demo.marker_status = results;
        params.volume = SAT_AUDIO_VOLUME_MAX;
        params.pan = SAT_AUDIO_PAN_CENTER;
        params.priority = 1u;
        params.pitch = SAT_FX16_ONE;
        params.flags = SAT_SOUND_PLAY_AT_TICK;
        params.start_tick = base + (MARKERS + 1u) * MARKER_SPACING;
        g_driver_demo.play_start_tick = params.start_tick;
        /* voice A starts on the tick and stays on */
        g_driver_demo.play_status = (uint32_t)(-sat_sound_play(tone, &params, &voice_a));
        g_driver_demo.voice_a_slot = voice_a.slot;
        /* voice B starts on the same tick and is keyed off by the driver later */
        g_driver_demo.play_status |= (uint32_t)(-sat_sound_play(tone, &params, &voice));
        g_driver_demo.voice_b_slot = voice.slot;
        g_driver_demo.key_off_status = (uint32_t)(-sat_sound_driver_schedule_key(
            base + (MARKERS + 2u) * MARKER_SPACING, (uint8_t)voice.slot, 0u));
    }

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_GREEN, &pad) != SAT_OK) break;
        ++g_driver_demo.frames;
        if (g_driver_demo.frames == 60u) {
            g_driver_demo.tick_a = sat_sound_driver_tick();
            g_driver_demo.frame_a = g_driver_demo.frames;
        }
        if (g_driver_demo.frames == 240u) {
            g_driver_demo.tick_b = sat_sound_driver_tick();
            g_driver_demo.frame_b = g_driver_demo.frames;
        }
        if (g_driver_demo.frames == 250u && g_driver_demo.running != 0u) {
            (void)sat_sound_driver_info(&info);
            g_driver_demo.heartbeat = info.heartbeat;
            g_driver_demo.executed = info.executed;
            g_driver_demo.max_lateness = info.max_lateness_ticks;
            g_driver_demo.queued_at_end = info.queued;
            for (uint32_t i = 0u; i < MARKERS + 3u; ++i) {
                uint16_t t = 0u, late = 0u;
                (void)sat_sound_driver_read_log(i, &t, &late);
                g_driver_demo.log_ticks[i] = t;
                g_driver_demo.log_late[i] = late;
            }
        }
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "SOUND DRIVER", 8, 8, 8, 0u, 0u);
        line("RUNNING ", g_driver_demo.running, 24);
        line("TICK ", sat_sound_driver_tick(), 40);
        line("EXECUTED ", g_driver_demo.executed, 56);
        line("MAX LATE ", g_driver_demo.max_lateness, 72);
        (void)sat_app_frame_end();
        (void)sat_audio_update();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_sound_driver_stop();
    (void)sat_shutdown();
    return 0;
}
