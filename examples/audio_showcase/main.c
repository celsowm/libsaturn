#include <stdint.h>

#include "saturn/app.h"
#include "saturn/asset.h"
#include "saturn/audio.h"
#include "saturn/color.h"
#include "saturn/example_util.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "audio_showcase/audio_data.h"

#define SHOWCASE_SFX_COUNT 4u

static sat_sound_t g_sounds[SHOWCASE_SFX_COUNT];
static sat_music_t g_music = {0};
static uint8_t g_music_playing = 0u;
static uint8_t g_selected = 0u;
static uint16_t g_volume = 255u;
static int16_t g_pan = 0;

static void draw_line(const sat_ascii_font_t* font, const char* text, int y) {
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(font, text, 8, y, 8, 0xFFFFu, 0u));
}

static void draw_value(const sat_ascii_font_t* font, const char* label, uint32_t value, int y) {
    char line[32];
    sat_example_must(sat_fmt_label_u32(label, value, line, sizeof(line), 0));
    draw_line(font, line, y);
}

static const char* selected_name(void) {
    switch (g_selected) {
        case 0u: return "CLICK";
        case 1u: return "LASER";
        case 2u: return "EXPLOSION";
        default: return "REFERENCE TONE";
    }
}

static sat_sound_play_params_t current_params(void) {
    sat_sound_play_params_t p;
    p.volume = g_volume;
    p.pan = g_pan;
    p.priority = 10u;
    p.flags = 0u;
    p.pitch = SAT_FX16_ONE;
    return p;
}

static void load_sound(sat_sound_t* out, const int16_t* samples, uint32_t count, uint8_t loop) {
    sat_sound_desc_t desc;
    desc.samples = samples;
    desc.sample_count = count;
    desc.sample_rate = SHOWCASE_SAMPLE_RATE;
    desc.loop_start = 0u;
    desc.loop_end = 0u;
    desc.format = SAT_AUDIO_PCM_S16;
    desc.loop = loop;
    desc.reserved0 = 0u;
    desc.reserved1 = 0u;
    sat_example_must(sat_sound_create(out, &desc));
}

static void toggle_music(void) {
    if (g_music_playing != 0u) {
        sat_example_must(sat_music_pause(g_music));
        g_music_playing = 0u;
        return;
    }

    sat_example_must(sat_music_resume(g_music));
    g_music_playing = 1u;
}

int main(void) {
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_audio_init());

    sat_ascii_font_t font;
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u));

    load_sound(&g_sounds[0], showcase_click, showcase_click_count, 0u);
    load_sound(&g_sounds[1], showcase_laser, showcase_laser_count, 0u);
    load_sound(&g_sounds[2], showcase_explosion, showcase_explosion_count, 0u);
    load_sound(&g_sounds[3], showcase_reference, showcase_reference_count, 0u);
    sat_asset_desc_t music_asset = {};
    music_asset.logical_path = "audio/showcase_music.satstream";
    music_asset.data = showcase_music;
    music_asset.size = showcase_music_count * sizeof(showcase_music[0]);
    music_asset.sample_rate = SHOWCASE_SAMPLE_RATE;
    music_asset.sample_count = showcase_music_count;
    music_asset.channels = 1u;
    music_asset.format = SAT_AUDIO_PCM_S16;
    music_asset.kind = SAT_ASSET_STREAM;
    sat_asset_t music_asset_handle;
    sat_example_must(sat_asset_register(&music_asset, &music_asset_handle));
    sat_example_must(sat_music_open(&g_music, music_asset.logical_path));

    sat_example_must(sat_music_play(g_music));
    g_music_playing = 1u;

    while (1) {
        sat_example_must(sat_music_update(g_music));
        sat_example_must(sat_audio_update());

        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));

        if ((pad.pressed & SAT_PAD_X) != 0u) {
            g_selected = (uint8_t)((g_selected + SHOWCASE_SFX_COUNT - 1u) % SHOWCASE_SFX_COUNT);
        }
        if ((pad.pressed & SAT_PAD_Y) != 0u) {
            g_selected = (uint8_t)((g_selected + 1u) % SHOWCASE_SFX_COUNT);
        }

        if ((pad.pressed & SAT_PAD_LEFT) != 0u && g_pan > SAT_AUDIO_PAN_LEFT) {
            g_pan = (int16_t)(g_pan - 3);
            if (g_pan < SAT_AUDIO_PAN_LEFT) g_pan = SAT_AUDIO_PAN_LEFT;
        }
        if ((pad.pressed & SAT_PAD_RIGHT) != 0u && g_pan < SAT_AUDIO_PAN_RIGHT) {
            g_pan = (int16_t)(g_pan + 3);
            if (g_pan > SAT_AUDIO_PAN_RIGHT) g_pan = SAT_AUDIO_PAN_RIGHT;
        }
        if ((pad.pressed & SAT_PAD_UP) != 0u) {
            g_volume = (uint16_t)(g_volume > 223u ? 255u : g_volume + 32u);
        }
        if ((pad.pressed & SAT_PAD_DOWN) != 0u) {
            g_volume = (uint16_t)(g_volume < 32u ? 0u : g_volume - 32u);
        }

        if ((pad.pressed & SAT_PAD_A) != 0u) {
            sat_sound_play_params_t p = current_params();
            sat_example_must(sat_sound_play(g_sounds[g_selected], &p, 0));
        }

        if ((pad.pressed & SAT_PAD_B) != 0u) {
            sat_sound_play_params_t p = current_params();
            for (uint16_t i = 0u; i < 36u; ++i) {
                p.pan = (int16_t)((int32_t)(i % 31u) - 15);
                p.priority = (uint16_t)(i & 3u);
                (void)sat_sound_play(g_sounds[i % SHOWCASE_SFX_COUNT], &p, 0);
            }
        }

        if ((pad.pressed & SAT_PAD_C) != 0u) {
            toggle_music();
        }

        if ((pad.held & SAT_PAD_L) != 0u && (pad.held & SAT_PAD_R) != 0u &&
            (pad.pressed & SAT_PAD_START) != 0u) {
            sat_example_must(sat_app_frame_end());
            break;
        }

        sat_audio_stats_t stats;
        sat_example_must(sat_audio_get_stats(&stats));
        sat_audio_stream_stats_t stream_stats;
        sat_example_must(sat_music_stats(g_music, &stream_stats));

        draw_line(&font, "LIBSATURN AUDIO SHOWCASE", 8);
        draw_line(&font, "A SFX  B BURST  C MUSIC", 24);
        draw_line(&font, "X/Y SOUND  LEFT/RIGHT PAN", 36);
        draw_line(&font, "UP/DOWN VOLUME", 48);
        draw_line(&font, selected_name(), 68);
        draw_value(&font, "REAL ASSETS ", SHOWCASE_REAL_ASSET_COUNT, 80);
        draw_value(&font, "VOLUME ", g_volume, 92);
        {
            char pan_line[24];
            char pan_num[SAT_FMT_I32_MAX];
            uint16_t n = 0u;
            uint16_t p = 0u;
            const char prefix[] = "PAN ";
            sat_example_must(sat_fmt_i32(g_pan, pan_num, sizeof(pan_num), &n));
            while (prefix[p] != '\0') { pan_line[p] = prefix[p]; ++p; }
            for (uint16_t i = 0u; i < n; ++i) pan_line[p + i] = pan_num[i];
            pan_line[p + n] = '\0';
            draw_line(&font, pan_line, 104);
        }
        draw_line(&font, g_music_playing != 0u ? "MUSIC ON" : "MUSIC OFF", 116);
        draw_value(&font, "VOICES ", stats.active_voices, 132);
        draw_value(&font, "STEALS ", stats.voice_steals, 144);
        draw_value(&font, "FAILED ", stats.failed_play_requests, 156);
        draw_value(&font, "STREAM BUF ", stream_stats.buffered_frames, 168);
        draw_value(&font, "STREAM UND ", stream_stats.underrun_count, 180);
        draw_value(&font, "STREAM REF ", stream_stats.refill_count, 192);
        draw_line(&font, "L+R+START EXIT", 208);

        sat_example_must(sat_app_frame_end());
    }

    (void)sat_music_close(g_music);
    for (uint8_t i = 0u; i < SHOWCASE_SFX_COUNT; ++i) {
        (void)sat_sound_unload(g_sounds[i]);
    }
    (void)sat_audio_shutdown();
    return 0;
}
