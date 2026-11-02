/* Seamless signed PCM8 streaming: stereo music from two mono SCSP streams.
 *
 * "8 Bit Disco Loop" (CC0, see examples/audio_showcase/LICENSES.md) is stored as one signed 8-bit array
 * per channel and fed, a few hundred frames per display frame, into two caller-owned 16 KiB rings that
 * loop it. The SCSP plays them in native PCM8; the SH-2 only copies bytes. A fires a resident PCM16
 * explosion over the music; B pauses/resumes the music (resume reopens the loop from the rings, which
 * refill within a few frames).
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/audio.h"
#include "saturn/color.h"
#include "example_util.h"
#include "saturn/font.h"
#include "saturn/hud.h"
#include "pcm8_stream/pcm8_data.h"

#define RING_FRAMES 16384u

static int8_t g_ring[2][RING_FRAMES];
static sat_audio_stream_t g_stream[2];
static uint32_t g_position;                 /* music frame the rings have been filled up to */

static void produce(void) {
    uint32_t n = RING_FRAMES;
    for (uint32_t k = 0u; k < 2u; ++k) {
        uint32_t room = sat_audio_stream_available(g_stream[k]);
        if (room < n) n = room;
    }
    if (n > 1024u) n = 1024u;               /* bounded work per display frame */
    if (n > PCM8_MUSIC_FRAMES - g_position) n = PCM8_MUSIC_FRAMES - g_position;   /* loop at the end */
    if (n == 0u) return;
    sat_example_must(sat_audio_stream_write(g_stream[0], pcm8_music_left + g_position, n));
    sat_example_must(sat_audio_stream_write(g_stream[1], pcm8_music_right + g_position, n));
    g_position += n;
    if (g_position == PCM8_MUSIC_FRAMES) g_position = 0u;
}

int main(void) {
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_audio_init());

    sat_ascii_font_t font;
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u));
    sat_hud_t hud;
    sat_example_must(sat_hud_init(&hud, &font, 0xFFFFu, 8u));

    sat_sound_desc_t desc = {0};
    desc.samples = pcm8_effect;
    desc.sample_count = PCM8_EFFECT_FRAMES;
    desc.sample_rate = PCM8_EFFECT_RATE;
    desc.format = SAT_AUDIO_PCM_S16;
    sat_sound_t beep;
    sat_example_must(sat_sound_create(&beep, &desc));

    sat_audio_spec_t spec = {PCM8_MUSIC_RATE, RING_FRAMES, 1u, SAT_AUDIO_PCM_S8, 0u};
    for (uint32_t k = 0u; k < 2u; ++k) {
        sat_example_must(sat_audio_stream_open(&g_stream[k], &spec, g_ring[k], sizeof g_ring[k]));
        sat_example_must(sat_audio_stream_set_pan(g_stream[k],
                                                  k == 0u ? SAT_AUDIO_PAN_LEFT : SAT_AUDIO_PAN_RIGHT));
    }

    uint8_t paused = 0u;
    while (1) {
        if (paused == 0u) produce();
        sat_example_must(sat_audio_update());

        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            sat_sound_play_params_t p;
            p.volume = 255u;
            p.pan = 0;
            p.priority = 10u;
            p.flags = 0u;
            p.pitch = SAT_FX16_ONE;
            (void)sat_sound_play(beep, &p, 0);
        }
        if ((pad.pressed & SAT_PAD_B) != 0u) {
            for (uint32_t k = 0u; k < 2u; ++k) {
                sat_example_must(paused == 0u ? sat_audio_stream_pause(g_stream[k])
                                              : sat_audio_stream_resume(g_stream[k]));
            }
            paused ^= 1u;
        }
        if ((pad.held & SAT_PAD_L) != 0u && (pad.held & SAT_PAD_R) != 0u &&
            (pad.pressed & SAT_PAD_START) != 0u) {
            sat_example_must(sat_app_frame_end());
            break;
        }

        sat_audio_stream_stats_t l, r;
        sat_example_must(sat_audio_stream_stats(g_stream[0], &l));
        sat_example_must(sat_audio_stream_stats(g_stream[1], &r));
        sat_example_must(sat_hud_text(&hud, "PCM8 STREAM STEREO", 8, 8));
        sat_example_must(sat_hud_text(&hud, "A SFX  B PAUSE", 8, 24));
        sat_example_must(sat_hud_text(&hud, paused != 0u ? "PAUSED" : "PLAYING", 8, 40));
        sat_example_must(sat_hud_value(&hud, "L REFILL ", l.refill_count, 8, 60));
        sat_example_must(sat_hud_value(&hud, "R REFILL ", r.refill_count, 8, 72));
        sat_example_must(sat_hud_value(&hud, "L UNDER ", l.underrun_count, 8, 84));
        sat_example_must(sat_hud_value(&hud, "R UNDER ", r.underrun_count, 8, 96));
        sat_example_must(sat_hud_value(&hud, "L BUF ", l.buffered_frames, 8, 108));
        sat_example_must(sat_hud_value(&hud, "R BUF ", r.buffered_frames, 8, 120));
        sat_example_must(sat_hud_value(&hud, "FRAMES ", g_position, 8, 140));
        sat_example_must(sat_hud_text(&hud, "L+R+START EXIT", 8, 208));
        sat_example_must(sat_app_frame_end());
    }

    for (uint32_t k = 0u; k < 2u; ++k) (void)sat_audio_stream_close(g_stream[k]);
    (void)sat_sound_unload(beep);
    (void)sat_audio_shutdown();
    return 0;
}
