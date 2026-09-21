/* runtime_2d - native high-level 2D acceptance surface.
 *
 * This example intentionally uses logical assets and the high-level runtime
 * APIs. It covers source regions, region prewarming, Camera2D state, clipping,
 * shapes, dynamic texture updates, baked text, input/events, a sound effect,
 * and a looping music stream without constructing VDP1 commands or touching
 * SCSP slots.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/asset.h"
#include "saturn/audio.h"
#include "saturn/color.h"
#include "saturn/example_util.h"
#include "saturn/font.h"
#include "saturn/fmt.h"
#include "saturn/input.h"
#include "saturn/render2d.h"
#include "saturn/surface.h"
#include "saturn/sprite_anim.h"
#include "saturn/time.h"

#define SPRITE_W 64u
#define SPRITE_H 32u
#define FRAME_W 16u
#define FRAME_H 16u
#define FONT_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 :/-"
#define FONT_GLYPH_COUNT ((uint16_t)(sizeof(FONT_CHARS) - 1u))
#define FONT_W ((uint16_t)(FONT_GLYPH_COUNT * SAT_ASCII_FONT_GLYPH_WIDTH))
#define FONT_H SAT_ASCII_FONT_GLYPH_HEIGHT

static uint8_t g_sprite_pixels[SPRITE_W * SPRITE_H];
static uint8_t g_dynamic_pixels[FRAME_W * FRAME_H];
static uint8_t g_font_pixels[FONT_W * FONT_H];
static uint16_t g_palette[256];
static sat_font_glyph_t g_font_glyphs[FONT_GLYPH_COUNT];
static int16_t g_tone[256];
static int16_t g_music_samples[1024];
static uint8_t g_data_blob[8] = {0x52u, 0x32u, 0x44u, 0x00u, 0x01u, 0x02u, 0x03u, 0x04u};
static const sat_rect_t g_player_frames[4] = {
    {0, 0, FRAME_W, FRAME_H}, {16, 0, FRAME_W, FRAME_H},
    {32, 0, FRAME_W, FRAME_H}, {48, 0, FRAME_W, FRAME_H}
};

static void build_assets(void) {
    for (uint16_t i = 0u; i < 256u; ++i) {
        g_palette[i] = SAT_BGR555((uint16_t)(i & 31u),
                                  (uint16_t)((i >> 2u) & 31u),
                                  (uint16_t)((i >> 4u) & 31u));
    }
    g_palette[0] = 0u;
    g_palette[1] = SAT_BGR555(31u, 31u, 31u);
    g_palette[2] = SAT_BGR555(31u, 4u, 4u);
    g_palette[3] = SAT_BGR555(4u, 31u, 8u);
    g_palette[4] = SAT_BGR555(4u, 10u, 31u);
    g_palette[5] = SAT_BGR555(31u, 25u, 3u);

    for (uint16_t y = 0u; y < SPRITE_H; ++y) {
        for (uint16_t x = 0u; x < SPRITE_W; ++x) {
            const uint16_t frame = (uint16_t)((x / FRAME_W) + (y / FRAME_H) * 4u);
            const uint16_t local_x = (uint16_t)(x % FRAME_W);
            const uint16_t local_y = (uint16_t)(y % FRAME_H);
            uint8_t value = (uint8_t)(1u + (frame % 4u));
            if (local_x == 0u || local_y == 0u || local_x == 15u || local_y == 15u) {
                value = 1u;
            } else if (((local_x + local_y + frame) & 3u) == 0u) {
                value = 5u;
            }
            g_sprite_pixels[y * SPRITE_W + x] = value;
        }
    }
    for (uint16_t i = 0u; i < FRAME_W * FRAME_H; ++i) {
        g_dynamic_pixels[i] = (uint8_t)((i + 1u) % 6u);
    }

    for (uint16_t glyph = 0u; glyph < FONT_GLYPH_COUNT; ++glyph) {
        const char character = FONT_CHARS[glyph];
        const uint8_t* rows = sat_font_ascii_8x8_rows(character);
        const uint16_t x0 = (uint16_t)(glyph * SAT_ASCII_FONT_GLYPH_WIDTH);
        for (uint16_t y = 0u; y < FONT_H; ++y) {
            for (uint16_t x = 0u; x < SAT_ASCII_FONT_GLYPH_WIDTH; ++x) {
                g_font_pixels[y * FONT_W + x0 + x] =
                    (uint8_t)((rows[y] & (uint8_t)(1u << (7u - x))) != 0u);
            }
        }
        g_font_glyphs[glyph].codepoint = (uint32_t)(uint8_t)character;
        g_font_glyphs[glyph].source = (sat_rect_t){(int16_t)x0, 0, 8u, 8u};
        g_font_glyphs[glyph].bearing_x = 0;
        g_font_glyphs[glyph].bearing_y = 0;
        g_font_glyphs[glyph].advance_x = 8;
        g_font_glyphs[glyph].reserved = 0u;
    }

    for (uint16_t i = 0u; i < 256u; ++i) {
        g_tone[i] = (int16_t)(((i & 31u) - 15) * 512);
    }
    for (uint16_t i = 0u; i < 1024u; ++i) {
        g_music_samples[i] = (int16_t)(((i & 63u) - 31) * 384);
    }
}

static void register_assets(void) {
    sat_asset_desc_t desc = {0};
    desc.logical_path = "textures/player-sheet";
    desc.data = g_sprite_pixels;
    desc.size = sizeof(g_sprite_pixels);
    desc.pitch = SPRITE_W;
    desc.width = SPRITE_W;
    desc.height = SPRITE_H;
    desc.palette_rgb555 = g_palette;
    desc.palette_count = 256u;
    desc.kind = SAT_ASSET_TEXTURE;
    sat_example_must(sat_asset_register(&desc, &(sat_asset_t){0}));

    desc = (sat_asset_desc_t){0};
    desc.logical_path = "fonts/runtime";
    desc.data = g_font_pixels;
    desc.size = sizeof(g_font_pixels);
    desc.pitch = FONT_W;
    desc.width = FONT_W;
    desc.height = FONT_H;
    desc.palette_rgb555 = g_palette;
    desc.palette_count = 256u;
    desc.glyphs = g_font_glyphs;
    desc.glyph_count = FONT_GLYPH_COUNT;
    desc.line_height = 8u;
    desc.fallback_glyph = 36u;
    desc.kind = SAT_ASSET_FONT;
    sat_example_must(sat_asset_register(&desc, &(sat_asset_t){0}));

    desc = (sat_asset_desc_t){0};
    desc.logical_path = "audio/click";
    desc.data = g_tone;
    desc.size = sizeof(g_tone);
    desc.sample_rate = 11025u;
    desc.sample_count = 256u;
    desc.channels = 1u;
    desc.format = SAT_AUDIO_PCM_S16;
    desc.kind = SAT_ASSET_SOUND;
    sat_example_must(sat_asset_register(&desc, &(sat_asset_t){0}));

    desc = (sat_asset_desc_t){0};
    desc.logical_path = "audio/runtime-music";
    desc.data = g_music_samples;
    desc.size = sizeof(g_music_samples);
    desc.sample_rate = 11025u;
    desc.sample_count = 1024u;
    desc.channels = 1u;
    desc.format = SAT_AUDIO_PCM_S16;
    desc.kind = SAT_ASSET_STREAM;
    sat_example_must(sat_asset_register(&desc, &(sat_asset_t){0}));

    desc = (sat_asset_desc_t){0};
    desc.logical_path = "data/runtime-metadata";
    desc.data = g_data_blob;
    desc.size = sizeof(g_data_blob);
    desc.kind = SAT_ASSET_DATA;
    sat_example_must(sat_asset_register(&desc, &(sat_asset_t){0}));
}

static void draw_hud(const sat_font_t* font, uint32_t now, uint32_t event_count,
                     uint32_t data_size, uint8_t music_on) {
    char line[40];
    sat_text_style_t style = sat_text_style_default();
    sat_example_must(sat_text_draw(font, "LIBSATURN RUNTIME 2D", 8, 5, &style));
    sat_example_must(sat_text_draw(font, "ARROWS MOVE  A SFX  B MUSIC  START EXIT", 8, 16, &style));
    sat_example_must(sat_text_draw(font, music_on != 0u ? "MUSIC ON" : "MUSIC OFF", 8, 205, &style));
    sat_example_must(sat_fmt_label_u32("MS ", now, line, sizeof(line), 0));
    sat_example_must(sat_text_draw(font, line, 8, 196, &style));
    sat_example_must(sat_fmt_label_u32("EVENTS ", event_count, line, sizeof(line), 0));
    sat_example_must(sat_text_draw(font, line, 112, 196, &style));
    sat_example_must(sat_fmt_label_u32("DATA ", data_size, line, sizeof(line), 0));
    sat_example_must(sat_text_draw(font, line, 208, 196, &style));
}

int main(void) {
    sat_texture_t player_sheet;
    sat_texture_t dynamic_texture;
    sat_font_t font;
    sat_sound_t sound;
    sat_music_t music;
    sat_surface_t dynamic_surface;
    sat_sprite_region_anim_t player_anim;
    const void* metadata = 0;
    uint32_t metadata_size = 0u;
    uint32_t event_count = 0u;
    uint32_t previous_ms = 0u;
    int16_t player_x = 160;
    int16_t player_y = 112;
    uint8_t music_on = 1u;

    sat_example_must(sat_app_init_default());
    sat_example_must(sat_audio_init());
    build_assets();
    register_assets();
    sat_example_must(sat_asset_load_data("data/runtime-metadata", &metadata, &metadata_size));
    (void)metadata;
    sat_example_must(sat_texture_load("textures/player-sheet", &player_sheet));
    sat_example_must(sat_sprite_region_anim_init(
        &player_anim, player_sheet, g_player_frames, 4u));
    for (uint16_t i = 0u; i < 4u; ++i)
        sat_example_must(sat_texture_prepare_region(player_sheet, &g_player_frames[i]));
    sat_example_must(sat_surface_init(
        &dynamic_surface, g_dynamic_pixels, FRAME_W, FRAME_H, FRAME_W,
        SAT_PIXEL_INDEX8, g_palette, 256u));
    sat_example_must(sat_texture_create_from_surface(
        &dynamic_texture, &dynamic_surface, SAT_TEXTURE_DYNAMIC));
    sat_example_must(sat_font_load("fonts/runtime", &font));
    sat_example_must(sat_font_prepare(&font));
    sat_example_must(sat_sound_load("audio/click", &sound));
    sat_example_must(sat_music_open(&music, "audio/runtime-music"));
    sat_example_must(sat_music_play(music));

    for (;;) {
        sat_pad_state_t pad = {0};
        const uint32_t now = sat_time_ms();
        const uint32_t elapsed = now - previous_ms;
        previous_ms = now;
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLUE, SAT_COLOR_BLACK, &pad));
        sat_event_t event;
        while (sat_event_poll(&event) > 0) ++event_count;

        if ((pad.held & SAT_PAD_LEFT) != 0u) player_x -= (int16_t)(elapsed / 16u + 1u);
        if ((pad.held & SAT_PAD_RIGHT) != 0u) player_x += (int16_t)(elapsed / 16u + 1u);
        if ((pad.held & SAT_PAD_UP) != 0u) player_y -= (int16_t)(elapsed / 16u + 1u);
        if ((pad.held & SAT_PAD_DOWN) != 0u) player_y += (int16_t)(elapsed / 16u + 1u);
        if (player_x < 32) player_x = 32;
        if (player_x > 288) player_x = 288;
        if (player_y < 48) player_y = 48;
        if (player_y > 176) player_y = 176;
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            sat_example_must(sat_sound_play(sound, 0, 0));
        }
        if ((pad.pressed & SAT_PAD_B) != 0u) {
            if (music_on != 0u) sat_example_must(sat_music_pause(music));
            else sat_example_must(sat_music_resume(music));
            music_on = (uint8_t)(music_on == 0u);
        }
        if ((pad.pressed & SAT_PAD_START) != 0u) break;

        for (uint16_t i = 0u; i < FRAME_W * FRAME_H; ++i) {
            g_dynamic_pixels[i] = (uint8_t)(1u + ((i + now / 32u) % 5u));
        }
        sat_example_must(sat_texture_update(dynamic_texture, &dynamic_surface));
        if (music_on != 0u) sat_example_must(sat_music_update(music));
        sat_example_must(sat_audio_update());

        sat_camera2d_t camera = sat_camera2d_default();
        camera.offset_x = 160 * SAT_FX16_ONE;
        camera.offset_y = 112 * SAT_FX16_ONE;
        camera.target_x = player_x * SAT_FX16_ONE;
        camera.target_y = player_y * SAT_FX16_ONE;
        sat_example_must(sat_render2d_push());
        sat_example_must(sat_render2d_set_camera(&camera));
        sat_example_must(sat_render2d_set_clip(&(sat_rect_t){16, 32, 288u, 160u}));
        sat_example_must(sat_fill_rect(&(sat_rect_t){0, 0, 320u, 224u}, sat_color_rgba(4u, 8u, 24u, 255u)));
        sat_example_must(sat_draw_rect(&(sat_rect_t){16, 32, 288u, 160u}, sat_color_rgba(31u, 31u, 31u, 255u)));
        sat_example_must(sat_draw_line((sat_point_t){0, 112}, (sat_point_t){320, 112}, sat_color_rgba(4u, 20u, 31u, 255u)));
        sat_draw_params_t params = sat_draw_params_default();
        params.rotation = (sat_fx16_t)((now % 360u) * 65536u / 360u);
        params.center = (sat_point_t){8, 8};
        params.flip = ((now / 700u) & 1u) != 0u ? SAT_FLIP_X : SAT_FLIP_NONE;
        sat_rect_t source;
        sat_example_must(sat_sprite_region_anim_set(
            &player_anim, (uint16_t)((now / 180u) & 3u)));
        sat_example_must(sat_sprite_region_anim_source(&player_anim, &source));
        const sat_rect_t destination = {(int16_t)(player_x - 8), (int16_t)(player_y - 8), 16u, 16u};
        sat_example_must(sat_draw_texture(player_sheet, &source, &destination, &params));
        sat_example_must(sat_draw_texture(dynamic_texture, 0, &(sat_rect_t){72, 72, 32u, 32u}, 0));
        sat_example_must(sat_render2d_set_clip(0));
        sat_example_must(sat_render2d_pop());
        draw_hud(&font, now, event_count, metadata_size, music_on);
        sat_example_must(sat_app_frame_end());
    }

    (void)sat_music_close(music);
    (void)sat_sound_unload(sound);
    (void)sat_texture_destroy(dynamic_texture);
    (void)sat_texture_destroy(player_sheet);
    (void)sat_texture_destroy(font.atlas);
    (void)sat_audio_shutdown();
    (void)sat_shutdown();
    return 0;
}
