/* SCSP DSP effects: a click through an echo, then through the reverb.
 * The click is a short burst of a square wave. It goes to the dry output and, at
 * full send, into the DSP. The echo repeats it every 100 ms at half the level; the
 * reverb repeats it at four fixed delays. g_dsp_demo and g_echo_ring hold what
 * harness/tests/test_scsp_dsp.py checks, next to the SCSP output the probe records. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/audio.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/scsp_dsp.h"
#include "saturn/time.h"
#include "saturn/video.h"

#define DSP_DEMO_MAGIC 0x44535031u /* "DSP1" */
#define CLICK_SAMPLES 64u
/* The sequence is timed in milliseconds, so it is the same at 60 and 50 frames per second, and it
 * repeats so a recording of any length holds a whole one. */
#define ECHO_START_MS 150u
#define ECHO_CLICK_MS 700u
#define ECHO_SNAPSHOT_MS 950u   /* 250 ms after the click: the ring holds the writes at 100 and 200 ms */
#define REVERB_START_MS 1500u
#define REVERB_CLICK_MS 1700u
#define CYCLE_MS 3000u
#define RING_WORDS 8192u

typedef struct dsp_demo_results {
    uint32_t magic;
    uint32_t audio_status;
    uint32_t echo_status;
    uint32_t echo_click_status;
    uint32_t echo_ring_offset;
    uint32_t echo_ring_bytes;
    uint32_t echo_delay_samples;
    uint32_t echo_kind;
    uint32_t snapshot_status;
    uint32_t reverb_status;
    uint32_t reverb_click_status;
    uint32_t reverb_kind;
    uint32_t reverb_ring_bytes;
    uint32_t stop_status;
    uint32_t kind_after_stop;
    uint32_t frames;
} dsp_demo_results_t;

volatile dsp_demo_results_t g_dsp_demo;
int16_t g_echo_ring[RING_WORDS];

static sat_ascii_font_t font;
static int16_t g_click[CLICK_SAMPLES];
static sat_sound_t click_sound;

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

static sat_result_t play_click(void) {
    sat_sound_play_params_t params = {0};
    sat_voice_t voice = {0};
    params.volume = SAT_AUDIO_VOLUME_MAX;
    params.pan = SAT_AUDIO_PAN_CENTER;
    params.priority = 1u;
    params.pitch = SAT_FX16_ONE;
    return sat_sound_play(click_sound, &params, &voice);
}

int main(void) {
    sat_sound_desc_t desc = {0};
    sat_effect_info_t info = {0};
    uint32_t have_click = 0u;
    uint32_t start_ms = 0u;
    uint32_t last_phase = 0u;
    uint32_t fired = 0u;   /* which events of this cycle have run */

    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_dsp_demo.magic = DSP_DEMO_MAGIC;

    /* 64 samples of a square wave, 8 samples per half period */
    for (uint32_t i = 0u; i < CLICK_SAMPLES; ++i) g_click[i] = (int16_t)(((i / 4u) & 1u) != 0u ? -12000 : 12000);
    desc.samples = g_click;
    desc.sample_count = CLICK_SAMPLES;
    desc.sample_rate = 44100u;
    desc.format = SAT_AUDIO_PCM_S16;
    desc.loop = 0u;

    g_dsp_demo.audio_status = (uint32_t)(-sat_audio_init());
    if (g_dsp_demo.audio_status == 0u && sat_sound_create(&click_sound, &desc) == SAT_OK) have_click = 1u;

    start_ms = sat_time_ms();
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_GREEN, &pad) != SAT_OK) break;
        ++g_dsp_demo.frames;
        const uint32_t phase = (sat_time_ms() - start_ms) % CYCLE_MS;
        if (phase < last_phase) fired = 0u;   /* a new cycle */
        last_phase = phase;

        if (have_click != 0u) {
            if (phase >= ECHO_START_MS && (fired & 1u) == 0u) {
                fired |= 1u;
                sat_effect_echo_params_t echo = {0};
                echo.delay_ms = 100u;
                echo.feedback = SAT_FX16_ONE / 2;
                echo.wet = (sat_fx16_t)(SAT_FX16_ONE * 6 / 10);
                echo.input_gain = SAT_FX16_ONE / 2;
                (void)sat_effect_set_default_send(7u);
                g_dsp_demo.echo_status = (uint32_t)(-sat_effect_echo_start(&echo));
                (void)sat_effect_info(&info);
                g_dsp_demo.echo_kind = info.kind;
                g_dsp_demo.echo_ring_offset = info.ring_offset;
                g_dsp_demo.echo_ring_bytes = info.ring_bytes;
                g_dsp_demo.echo_delay_samples = info.delay_samples;
            }
            if (phase >= ECHO_CLICK_MS && (fired & 2u) == 0u) {
                fired |= 2u;
                g_dsp_demo.echo_click_status = (uint32_t)(-play_click());
            }
            if (phase >= ECHO_SNAPSHOT_MS && (fired & 4u) == 0u) {
                fired |= 4u;
                /* 250 ms after the click: the ring holds the second and third writes of the line */
                g_dsp_demo.snapshot_status = (uint32_t)(-sat_effect_read_ring(0u, g_echo_ring, RING_WORDS));
                g_dsp_demo.stop_status = (uint32_t)(-sat_effect_stop());
                (void)sat_effect_info(&info);
                g_dsp_demo.kind_after_stop = info.kind;
            }
            if (phase >= REVERB_START_MS && (fired & 8u) == 0u) {
                fired |= 8u;
                sat_effect_reverb_params_t reverb = {0};
                reverb.feedback = (sat_fx16_t)(SAT_FX16_ONE * 78 / 100);
                reverb.wet = SAT_FX16_ONE / 4;
                reverb.input_gain = (sat_fx16_t)(SAT_FX16_ONE * 35 / 100);
                g_dsp_demo.reverb_status = (uint32_t)(-sat_effect_reverb_start(&reverb));
                (void)sat_effect_info(&info);
                g_dsp_demo.reverb_kind = info.kind;
                g_dsp_demo.reverb_ring_bytes = info.ring_bytes;
            }
            if (phase >= REVERB_CLICK_MS && (fired & 16u) == 0u) {
                fired |= 16u;
                g_dsp_demo.reverb_click_status = (uint32_t)(-play_click());
            }
        }

        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "SCSP DSP", 8, 8, 8, 0u, 0u);
        line("ECHO ", g_dsp_demo.echo_kind, 24);
        line("REVERB ", g_dsp_demo.reverb_kind, 40);
        line("FRAME ", g_dsp_demo.frames, 56);
        (void)sat_app_frame_end();
        (void)sat_audio_update();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_effect_stop();
    (void)sat_shutdown();
    return 0;
}
