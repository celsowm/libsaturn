/* infinite_explorer - procedural 360-degree VDP2 world. */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "explorer_logic.h"
#include "infinite_explorer/terrain.h"
#include "infinite_explorer/horizon.h"
#include "infinite_explorer/spacecraft.h"
#include "infinite_explorer/audio_data.h"

#define BM_BASE_WORD   0x00000u
#define RP_BASE_WORD   0x10000u
#define COEF_BASE_WORD 0x12000u
#define SKY_W EXPLORER_HORIZON_WIDTH
#define SKY_H EXPLORER_HORIZON_HEIGHT
#define MAX_RENDER 60
#define FX(v) ((int32_t)((v) * 65536))

typedef struct landmark {
    int32_t cx, cz;
    int32_t lx, lz;
    uint8_t active;
} landmark_t;

typedef struct render_item {
    explorer_projection_t p;
    int32_t side, depth;
    uint8_t type, biome, mission;
} render_item_t;

static uint16_t g_map_scratch[SAT_VDP2_NBG0_MAP_CELLS];
static uint16_t g_ground_palette[256], g_sky_palette[256];
static sat_ascii_font_t g_font;
static explorer_pos_t g_player;
static landmark_t g_towers[3], g_portal;
static render_item_t g_render[MAX_RENDER];
static sat_texture_t g_world_texture;
static sat_mat4_t g_view_proj;
static uint32_t g_seed = 0x51A7C0DEu, g_frames, g_distance;
static int32_t g_heading, g_speed, g_heading_delta;
static uint16_t g_render_count, g_drones;
static uint8_t g_energy, g_artifacts, g_biome, g_target_biome;
static uint8_t g_transition, g_invulnerable, g_paused, g_finished;
static uint8_t g_scanner;
static int8_t g_signal_dir;
static uint32_t g_signal_distance;
static uint16_t g_scanner_cooldown;
static uint8_t g_weather_audio;

static sat_sound_t g_snd_engine, g_snd_scanner, g_snd_fire, g_snd_impact, g_snd_music, g_snd_storm;
static sat_voice_t g_voice_engine, g_voice_music, g_voice_storm, g_voice_portal;
static uint8_t g_audio_ready;

static uint16_t scale_color(uint16_t c, uint8_t scale) {
    uint16_t r = (uint16_t)((c & 31u) * scale / 31u);
    uint16_t g = (uint16_t)(((c >> 5) & 31u) * scale / 31u);
    uint16_t b = (uint16_t)(((c >> 10) & 31u) * scale / 31u);
    return SAT_BGR555(r, g, b);
}

static uint16_t tint_horizon(uint16_t c, uint8_t biome, uint8_t scale) {
    uint16_t r = c & 31u;
    uint16_t g = (c >> 5) & 31u;
    uint16_t b = (c >> 10) & 31u;
    if (biome == 0u) {
        r = (uint16_t)(r * 26u / 31u);
        b = (uint16_t)((b * 31u + 15u) / 31u);
    } else if (biome == 1u) {
        g = (uint16_t)(g * 25u / 31u);
        b = (uint16_t)(b * 20u / 31u);
    } else {
        g = (uint16_t)(g * 22u / 31u);
        r = (uint16_t)((r * 31u + 15u) / 31u);
    }
    r = (uint16_t)(r * scale / 31u);
    g = (uint16_t)(g * scale / 31u);
    b = (uint16_t)(b * scale / 31u);
    return SAT_BGR555(r, g, b);
}

/* Both layers keep colour index 0 exactly as the converter left it.
 *
 * Zeroing it here used to speckle the ground black. Both layers run with
 * transparent-code processing OFF (BGON reads 0x1111: N0TPON and R0TPON set),
 * so colour index 0 is not transparent -- it is drawn with whatever palette
 * entry 0 holds. The terrain was quantized WITHOUT reserving index 0, so
 * about a fifth of its pixels landed on it, and forcing that entry to black
 * turned every one of them into a black dot on the rock.
 *
 * tools/convert_indexed8.py is now told --reserve-index0, so the terrain
 * never uses index 0 at all and there is nothing to suppress. Reserving it
 * is worth doing even here, where nothing is transparent: it costs one
 * colour out of 256 and makes the asset safe to reuse on the VDP1, which
 * does treat index 0 as transparent unconditionally. */
static void upload_biome_palettes(uint8_t biome, uint8_t scale) {
    uint16_t i;
    for (i = 0; i < 256u; ++i) {
        g_ground_palette[i] = scale_color(terrain_palette[i], scale);
        g_sky_palette[i] = tint_horizon(explorer_horizon_palette[i], biome, scale);
    }
    sat_example_must(sat_vdp2_palette_upload(g_ground_palette, 256u, 0u));
    sat_example_must(sat_vdp2_palette_upload(g_sky_palette, 256u, 256u));
}

static void generate_ground(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t y, x, off = BM_BASE_WORD;
    for (y = 0; y < 256u; ++y) {
        for (x = 0; x < 512u; x += 2u) {
            uint8_t a = terrain_pixels[(y & 127u) * 128u + (x & 127u)];
            uint8_t b = terrain_pixels[(y & 127u) * 128u + ((x + 1u) & 127u)];
            vram[off++] = (uint16_t)(((uint16_t)a << 8u) | b);
        }
    }
}

static void init_textured_3d(void) {
    sat_vec3_t eye = {0, FX(34), FX(-42)}, target = {0, FX(18), FX(90)}, up = {0,FX(1),0};
    sat_mat4_t view, proj;
    sat_example_must(sat_tex_upload_indexed8(&g_world_texture,spacecraft_pixels,64u,64u,spacecraft_palette,3u));
    sat_example_must(sat_mat4_look_at(&view,&eye,&target,&up));
    sat_example_must(sat_mat4_perspective(&proj,FX(56),sat_fx16_div(FX(320),FX(224)),FX(4),FX(1300)));
    sat_example_must(sat_mat4_multiply(&g_view_proj,&proj,&view));
}

static void write_coefficients(void) {
    uint16_t words[224u * 2u];
    uint32_t y;
    for (y = 0; y < 224u; ++y) {
        if (y <= EXPLORER_HORIZON) { words[y * 2u] = 0x8000u; words[y * 2u + 1u] = 0; }
        else {
            uint32_t d = (y - EXPLORER_HORIZON) + 8u;
            uint32_t k = ((EXPLORER_FOCAL << 16) + d / 2u) / d;
            words[y * 2u] = (uint16_t)((k >> 16) & 0x7Fu);
            words[y * 2u + 1u] = (uint16_t)k;
        }
    }
    sat_example_must(sat_vdp2_vram_write_words(COEF_BASE_WORD, words, 224u * 2u));
}

static void init_layers(void) {
    sat_vdp2_nbg0_config_t sky = {SAT_VDP2_CHAR_SIZE_1X1, SAT_VDP2_COLOR_MODE_256, 0x3Bu, 0u, 0u};
    sat_vdp2_rbg0_mode7_config_t ground = {SAT_VDP2_RBG0_BITMAP_512x256,
        SAT_VDP2_COLOR_MODE_256, BM_BASE_WORD, RP_BASE_WORD, SAT_COLOR_BLACK, 5u, 7u};
    uint16_t params[48];
    generate_ground();
    upload_biome_palettes(0u, 31u);
    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(
        explorer_horizon_pixels, SKY_W, SKY_H, 1u, g_map_scratch));
    write_coefficients();
    explorer_build_ground_params(FX(128), FX(128), 0, FX(1), COEF_BASE_WORD, params);
    sat_example_must(sat_vdp2_vram_write_words(RP_BASE_WORD, params, 48u));
    sat_example_must(sat_vdp2_rbg0_mode7_init(&ground));
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_rbg0_set_priority(5u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
}

static void load_sound(sat_sound_t* out, const int8_t* samples, uint32_t count, uint8_t loop) {
    sat_sound_desc_t desc;
    desc.samples = samples;
    desc.sample_count = count;
    desc.sample_rate = EXPLORER_AUDIO_SAMPLE_RATE;
    desc.loop_start = 0u;
    desc.loop_end = 0u;
    desc.format = SAT_AUDIO_PCM_S8;
    desc.loop = loop;
    desc.reserved0 = 0u;
    desc.reserved1 = 0u;
    sat_example_must(sat_sound_create(out, &desc));
}

static sat_sound_play_params_t sound_params(uint16_t volume, int16_t pan, uint16_t priority, sat_fx16_t pitch) {
    sat_sound_play_params_t p;
    p.volume = volume;
    p.pan = pan;
    p.priority = priority;
    p.flags = 0u;
    p.pitch = pitch;
    return p;
}

static void play_one_shot(sat_sound_t sound, uint16_t volume, int16_t pan, uint16_t priority, sat_fx16_t pitch) {
    sat_sound_play_params_t p = sound_params(volume, pan, priority, pitch);
    (void)sat_sound_play(sound, &p, 0);
}

static void start_loop(sat_sound_t sound, uint16_t volume, int16_t pan, uint16_t priority,
                       sat_fx16_t pitch, sat_voice_t* voice) {
    sat_sound_play_params_t p = sound_params(volume, pan, priority, pitch);
    sat_example_must(sat_sound_play(sound, &p, voice));
}

static void init_audio(void) {
    sat_example_must(sat_audio_init());
    load_sound(&g_snd_engine, explorer_engine, explorer_engine_count, 1u);
    load_sound(&g_snd_scanner, explorer_scanner, explorer_scanner_count, 0u);
    load_sound(&g_snd_fire, explorer_fire, explorer_fire_count, 0u);
    load_sound(&g_snd_impact, explorer_impact, explorer_impact_count, 0u);
    load_sound(&g_snd_music, explorer_music, explorer_music_count, 1u);
    load_sound(&g_snd_storm, explorer_storm, explorer_storm_count, 1u);
    start_loop(g_snd_music, 62u, 0, 240u, SAT_FX16_ONE, &g_voice_music);
    start_loop(g_snd_engine, 22u, 0, 230u, SAT_FX16_ONE, &g_voice_engine);
    g_audio_ready = 1u;
}

static int32_t signed_range(uint32_t h, uint32_t min, uint32_t span) {
    int32_t v = (int32_t)(min + h % span);
    return (h & 0x80000000u) ? -v : v;
}

static void reset_game(void) {
    uint8_t i;
    if (g_audio_ready) {
        if (sat_voice_is_playing(g_voice_storm)) (void)sat_voice_stop(g_voice_storm);
        if (sat_voice_is_playing(g_voice_portal)) (void)sat_voice_stop(g_voice_portal);
    }
    g_seed = g_seed * 1664525u + 1013904223u + g_frames;
    g_player.chunk_x = g_player.chunk_z = 0;
    g_player.local_x = g_player.local_z = FX(128);
    g_heading = g_speed = g_heading_delta = 0;
    g_frames = g_distance = g_drones = 0;
    g_energy = 6; g_artifacts = 0; g_invulnerable = 0; g_finished = 0; g_paused = 0;
    g_biome = g_target_biome = explorer_biome(g_seed, 0, 0); g_transition = 0;
    g_signal_distance = 0xFFFFFFFFu; g_scanner_cooldown = 0u; g_weather_audio = 0u;
    for (i = 0; i < 3u; ++i) {
        uint32_t h = explorer_hash(g_seed, i + 11, i * 7);
        g_towers[i].cx = signed_range(h, 2u + i, 3u);
        g_towers[i].cz = signed_range(h >> 3, 2u + i, 3u);
        g_towers[i].lx = FX(48 + ((h >> 8) & 159u));
        g_towers[i].lz = FX(48 + ((h >> 16) & 159u));
        g_towers[i].active = 0;
    }
    g_portal.active = 0;
    upload_biome_palettes(g_biome, 31u);
}

static uint16_t object_color(uint8_t biome, uint8_t bright) {
    if (biome == 0) return bright ? SAT_RGB555(5,31,31) : SAT_RGB555(2,10,15);
    if (biome == 1) return bright ? SAT_RGB555(31,24,4) : SAT_RGB555(16,7,2);
    return bright ? SAT_RGB555(31,4,24) : SAT_RGB555(14,2,8);
}

static void add_render(int32_t cx, int32_t cz, int32_t lx, int32_t lz,
                       uint8_t type, uint8_t biome, uint8_t mission,
                       int32_t sin_h, int32_t cos_h) {
    if (g_render_count >= MAX_RENDER) return;
    int32_t dx = explorer_relative_fx(cx, lx, g_player.chunk_x, g_player.local_x);
    int32_t dz = explorer_relative_fx(cz, lz, g_player.chunk_z, g_player.local_z);
    explorer_projection_t p = explorer_project(dx, dz, sin_h, cos_h);
    if (!p.visible) return;
    {
        int32_t side = (int32_t)(((int64_t)dx*cos_h - (int64_t)dz*sin_h) >> 16);
        int32_t depth = (int32_t)(((int64_t)dx*sin_h + (int64_t)dz*cos_h) >> 16);
        g_render[g_render_count++] = (render_item_t){p, side, depth, type, biome, mission};
    }
}

static void build_render_list(int32_t sin_h, int32_t cos_h) {
    int32_t dz, dx;
    uint8_t i;
    g_render_count = 0;
    for (dz = -2; dz <= 2; ++dz) for (dx = -2; dx <= 2; ++dx) {
        int32_t cx = g_player.chunk_x + dx, cz = g_player.chunk_z + dz;
        uint32_t h = explorer_hash(g_seed, cx, cz);
        uint8_t b = explorer_biome(g_seed, cx, cz);
        add_render(cx, cz, FX(32 + (h & 191u)), FX(32 + ((h >> 8) & 191u)), (uint8_t)(h % 3u), b, 0, sin_h, cos_h);
        add_render(cx, cz, FX(32 + ((h >> 16) & 191u)), FX(32 + ((h >> 24) & 191u)), (uint8_t)(1u + ((h >> 5) % 3u)), b, 0, sin_h, cos_h);
    }
    add_render(0,0,FX(72),FX(205),0u,g_biome,0u,sin_h,cos_h);
    add_render(0,0,FX(186),FX(188),1u,g_biome,0u,sin_h,cos_h);
    add_render(0,0,FX(128),FX(232),2u,g_biome,0u,sin_h,cos_h);
    for (i = 0; i < 3u; ++i) if (!g_towers[i].active)
        add_render(g_towers[i].cx, g_towers[i].cz, g_towers[i].lx, g_towers[i].lz, 4u,
                   explorer_biome(g_seed, g_towers[i].cx, g_towers[i].cz), 1u, sin_h, cos_h);
    if (g_portal.active) add_render(g_portal.cx, g_portal.cz, g_portal.lx, g_portal.lz, 5u, g_biome, 1u, sin_h, cos_h);
    for (i = 1; i < g_render_count; ++i) {
        render_item_t item = g_render[i]; int j = i - 1;
        while (j >= 0 && g_render[j].p.depth < item.p.depth) { g_render[j + 1] = g_render[j]; --j; }
        g_render[j + 1] = item;
    }
}

static void draw_landmark(const render_item_t* r) {
    int16_t s = r->p.size < 5 ? 5 : r->p.size;
    int16_t x = r->p.x;
    int16_t y = r->p.ground_y;
    uint16_t bright = object_color(r->biome, 1u);
    uint16_t dark = object_color(r->biome, 0u);
    if (r->type == 4u) {
        int16_t h = (int16_t)(s * 3);
        int16_t w = (int16_t)(s / 3 + 2);
        sat_draw_rect_screen((int16_t)(x - w), (int16_t)(y - h), (uint16_t)(w * 2), (uint16_t)h, dark);
        sat_draw_rect_screen((int16_t)(x - 1), (int16_t)(y - h - 5), 3u, (uint16_t)(h + 5), bright);
        sat_draw_rect_screen((int16_t)(x - s), (int16_t)(y - h / 2), (uint16_t)(s * 2), 2u, bright);
    } else {
        int16_t w = (int16_t)(s * 2 + 8);
        int16_t h = (int16_t)(s * 3 + 12);
        uint16_t pulse = ((g_frames >> 3) & 1u) ? bright : SAT_COLOR_WHITE;
        sat_draw_rect_screen((int16_t)(x - w / 2), (int16_t)(y - h), 3u, (uint16_t)h, pulse);
        sat_draw_rect_screen((int16_t)(x + w / 2 - 3), (int16_t)(y - h), 3u, (uint16_t)h, pulse);
        sat_draw_rect_screen((int16_t)(x - w / 2), (int16_t)(y - h), (uint16_t)w, 3u, pulse);
        sat_draw_rect_screen((int16_t)(x - w / 2), (int16_t)(y - 3), (uint16_t)w, 3u, dark);
    }
}

/* Scenery rock colour.
 *
 * Deliberately NOT object_color(): that returns the saturated biome accent
 * used for mission markers and the HUD, and a field of rocks in it reads as a
 * row of neon boxes rather than as ground. These are stone tints that sit
 * close to the terrain bitmap, nudged per biome so a biome change is still
 * legible. */
static uint16_t rock_color(uint8_t biome, uint8_t bright) {
    if (biome == 0u) return bright ? SAT_RGB555(14, 15, 17) : SAT_RGB555(6, 7, 9);
    if (biome == 1u) return bright ? SAT_RGB555(18, 15, 10) : SAT_RGB555(8, 6, 4);
    return bright ? SAT_RGB555(16, 12, 14) : SAT_RGB555(7, 5, 6);
}

/* A squat, flat-shaded silhouette, lit from one side so a field of them still
 * reads as ground clutter.
 *
 * These used to be drawn with the spacecraft texture -- the same 64x64 NASA
 * render as the player's craft and the drones -- which is why the world was
 * full of identical dark rectangles instead of anything recognisable. A rock
 * has no texture to be drawn with, so it is a shaded polygon. */
static void draw_rock(const render_item_t* r, int half, int height) {
    sat_fx16_t w = FX(half);
    sat_fx16_t h = FX(height);
    sat_quad3_t body;
    uint16_t gouraud[4];
    /* Corner order is A top-left, B top-right, C bottom-right, D bottom-left.
     * Narrowing the top, asymmetrically, turns the billboard rectangle into a
     * boulder silhouette -- a plain rectangle reads as a floating card however
     * it is shaded. */
    body.v[0].x = r->side - (w * 7 / 16); body.v[0].y = h;  body.v[0].z = r->depth;
    body.v[1].x = r->side + (w * 9 / 16); body.v[1].y = h;  body.v[1].z = r->depth;
    body.v[2].x = r->side + w;            body.v[2].y = 0;  body.v[2].z = r->depth;
    body.v[3].x = r->side - w;            body.v[3].y = 0;  body.v[3].z = r->depth;
    /* Lit from the left, so a whole field of them agrees on where the sun is. */
    gouraud[0] = sat_gouraud_from_intensity(SAT_FX16_ONE);
    gouraud[1] = sat_gouraud_from_intensity(SAT_FX16_ONE / 3);
    gouraud[2] = sat_gouraud_from_intensity(SAT_FX16_ONE / 5);
    gouraud[3] = sat_gouraud_from_intensity(SAT_FX16_ONE * 3 / 4);

    {
        /* Contact shadow, submitted BEFORE the boulder: the VDP1 has no depth
         * buffer and paints in list order, so the later command wins wherever
         * the two overlap. Without it the boulder floats. */
        sat_quad3_t ground;
        sat_quad3_floor(&ground, r->side, 0, r->depth, w);
        (void)sat_draw_world_polygon(&g_view_proj, &ground, rock_color(r->biome, 0u));
    }
    (void)sat_draw_world_polygon_gouraud(&g_view_proj, &body, rock_color(r->biome, 1u),
                                         gouraud);
}

static void draw_world(void) {
    uint16_t i;
    for (i = 0; i < g_render_count; ++i) {
        render_item_t* r = &g_render[i];
        if (r->type == 4u || r->type == 5u) {
            draw_landmark(r);
        } else if (r->type == 3u) {
            /* Drone: a real craft, so it gets the real craft texture, hovering
             * clear of the ground. */
            sat_quad3_t q;
            uint8_t v;
            sat_quad3_billboard(&q, r->side, r->depth, FX(1), 0, FX(12), FX(26));
            for (v = 0; v < 4u; ++v) q.v[v].y += FX(18);
            (void)sat_draw_world_sprite(&g_view_proj, &q, &g_world_texture, 0, 0);
        } else {
            /* Boulders, not buildings: the old sizes came from a billboard
             * that was mostly transparent texture, so as solid geometry they
             * filled the screen. */
            draw_rock(r, r->type == 2u ? 8 : (r->type == 1u ? 5 : 6),
                      r->type == 2u ? 13 : 9);
        }
    }
}

static int near_landmark(const landmark_t* l, int radius) {
    int32_t dx = explorer_relative_fx(l->cx, l->lx, g_player.chunk_x, g_player.local_x) >> 16;
    int32_t dz = explorer_relative_fx(l->cz, l->lz, g_player.chunk_z, g_player.local_z) >> 16;
    return dx * dx + dz * dz < radius * radius;
}

static uint32_t abs_u32(int32_t v) {
    return (uint32_t)(v < 0 ? -v : v);
}

static void update_scanner(int32_t sin_h, int32_t cos_h) {
    uint8_t i, found = 0; int64_t best = 0x7FFFFFFFFFFFFFFFLL; int32_t best_dx = 0, best_dz = 0;
    for (i = 0; i < 3u; ++i) if (!g_towers[i].active) {
        int32_t dx = explorer_relative_fx(g_towers[i].cx,g_towers[i].lx,g_player.chunk_x,g_player.local_x);
        int32_t dz = explorer_relative_fx(g_towers[i].cz,g_towers[i].lz,g_player.chunk_z,g_player.local_z);
        int64_t d = (int64_t)(dx >> 12) * (dx >> 12) + (int64_t)(dz >> 12) * (dz >> 12);
        if (d < best) { best = d; best_dx = dx; best_dz = dz; found = 1; }
    }
    if (!found && g_portal.active) {
        best_dx = explorer_relative_fx(g_portal.cx,g_portal.lx,g_player.chunk_x,g_player.local_x);
        best_dz = explorer_relative_fx(g_portal.cz,g_portal.lz,g_player.chunk_z,g_player.local_z);
        found = 1;
    }
    if (found) {
        int32_t side = (int32_t)(((int64_t)best_dx*cos_h - (int64_t)best_dz*sin_h) >> 16);
        int32_t front = (int32_t)(((int64_t)best_dx*sin_h + (int64_t)best_dz*cos_h) >> 16);
        g_signal_dir = (int8_t)(side < -FX(8) ? -1 : (side > FX(8) ? 1 : (front >= 0 ? 0 : 2)));
        g_signal_distance = (abs_u32(best_dx >> 16) + abs_u32(best_dz >> 16)) / 2u;
    } else {
        g_signal_dir = 0;
        g_signal_distance = 0xFFFFFFFFu;
    }
}

static void update_audio_reactive(const sat_pad_state_t* pad) {
    uint8_t weather;
    uint32_t speed;
    uint16_t engine_volume;
    if (!g_audio_ready) return;
    sat_example_must(sat_audio_update());

    speed = abs_u32(g_speed) >> 16;
    engine_volume = (uint16_t)(22u + speed * 26u + ((pad->held & SAT_PAD_B) ? 34u : 0u));
    if (engine_volume > 180u) engine_volume = 180u;
    if (g_paused || g_finished) engine_volume = 6u;
    if (sat_voice_is_playing(g_voice_engine)) (void)sat_voice_set_volume(g_voice_engine, engine_volume);
    if (sat_voice_is_playing(g_voice_music)) (void)sat_voice_set_volume(g_voice_music, g_paused ? 26u : 62u);

    if (g_scanner && !g_finished && g_signal_distance != 0xFFFFFFFFu) {
        if (g_scanner_cooldown == 0u) {
            int16_t pan = g_signal_dir < 0 ? -12 : (g_signal_dir == 1 ? 12 : 0);
            sat_fx16_t pitch = g_signal_distance < 90u ? SAT_FX16_ONE + SAT_FX16_ONE / 2 :
                                 (g_signal_distance < 220u ? SAT_FX16_ONE + SAT_FX16_ONE / 4 : SAT_FX16_ONE);
            play_one_shot(g_snd_scanner, 150u, pan, 160u, pitch);
            g_scanner_cooldown = g_signal_distance < 90u ? 8u : (g_signal_distance < 220u ? 16u : 28u);
        } else {
            --g_scanner_cooldown;
        }
    } else {
        g_scanner_cooldown = 0u;
    }

    weather = (uint8_t)((g_frames / 600u) % 4u);
    if (weather == 3u && g_weather_audio != 3u) {
        start_loop(g_snd_storm, 72u, 0, 220u, SAT_FX16_ONE, &g_voice_storm);
    } else if (weather != 3u && g_weather_audio == 3u && sat_voice_is_playing(g_voice_storm)) {
        (void)sat_voice_stop(g_voice_storm);
    }
    g_weather_audio = weather;

    if (g_portal.active) {
        if (!sat_voice_is_playing(g_voice_portal))
            start_loop(g_snd_engine, 68u, 0, 210u, SAT_FX16_ONE / 2, &g_voice_portal);
    } else if (sat_voice_is_playing(g_voice_portal)) {
        (void)sat_voice_stop(g_voice_portal);
    }
}

static void update_game(const sat_pad_state_t* pad) {
    int32_t sin_h, cos_h, thrust = 0;
    uint8_t i;
    if (pad->pressed & SAT_PAD_START) {
        if (g_audio_ready) play_one_shot(g_snd_scanner, 90u, 0, 200u, SAT_FX16_ONE / 2);
        if (g_finished) { reset_game(); return; }
        g_paused ^= 1u;
    }
    if (g_paused || g_finished) return;
    g_heading_delta = 0;
    if (pad->held & SAT_PAD_LEFT) { g_heading -= FX(2); g_heading_delta = -1; }
    if (pad->held & SAT_PAD_RIGHT) { g_heading += FX(2); g_heading_delta = 1; }
    while (g_heading < 0) g_heading += FX(360);
    while (g_heading >= FX(360)) g_heading -= FX(360);
    if (pad->held & SAT_PAD_UP) thrust += FX(1) / 12;
    if (pad->held & SAT_PAD_DOWN) thrust -= FX(1) / 10;
    if ((pad->held & SAT_PAD_B) && g_energy > 1u) { thrust += FX(1) / 7; if ((g_frames & 31u) == 0u) --g_energy; }
    g_speed += thrust; g_speed = (int32_t)(((int64_t)g_speed * 61) / 64);
    if (g_speed > FX(5)) g_speed = FX(5);
    if (g_speed < FX(-2)) g_speed = FX(-2);
    sin_h = sat_sin_deg(g_heading); cos_h = sat_cos_deg(g_heading);
    g_scanner = (pad->held & SAT_PAD_C) != 0u;
    update_scanner(sin_h, cos_h);
    g_player.local_x += sat_fx16_mul(sin_h, g_speed);
    g_player.local_z += sat_fx16_mul(cos_h, g_speed);
    explorer_rebase(&g_player);
    g_distance += (uint32_t)(g_speed < 0 ? -g_speed : g_speed) >> 16;
    if (g_invulnerable) --g_invulnerable;
    for (i = 0; i < 3u; ++i) if (!g_towers[i].active && near_landmark(&g_towers[i], 22) && (pad->held & SAT_PAD_C)) {
        g_towers[i].active = 1; ++g_artifacts; g_energy = 6;
        if (g_audio_ready) play_one_shot(g_snd_scanner, 205u, 0, 190u,
            SAT_FX16_ONE + (sat_fx16_t)(g_artifacts * (SAT_FX16_ONE / 8)));
        if (g_artifacts == 3u) {
            g_portal.cx = g_player.chunk_x + (sin_h >= 0 ? 2 : -2);
            g_portal.cz = g_player.chunk_z + (cos_h >= 0 ? 2 : -2);
            g_portal.lx = g_portal.lz = FX(128); g_portal.active = 1;
            if (g_audio_ready) play_one_shot(g_snd_impact, 130u, 0, 210u, SAT_FX16_ONE / 2);
        }
    }
    if (g_portal.active && near_landmark(&g_portal, 28) && (pad->held & SAT_PAD_C)) {
        if (g_audio_ready) play_one_shot(g_snd_scanner, 220u, 0, 230u, SAT_FX16_ONE * 2);
        g_finished = 2;
    }
    if ((pad->pressed & SAT_PAD_A) && ((explorer_hash(g_seed, g_player.chunk_x, g_player.chunk_z) & 3u) != 0u)) {
        ++g_drones;
        if (g_audio_ready) play_one_shot(g_snd_fire, 175u, 0, 70u, SAT_FX16_ONE);
    }
    {
        uint32_t h = explorer_hash(g_seed,g_player.chunk_x,g_player.chunk_z);
        int32_t ox = FX(32 + (h & 191u)), oz = FX(32 + ((h >> 8) & 191u));
        int32_t dx = (ox - g_player.local_x) >> 16, dz = (oz - g_player.local_z) >> 16;
        if (dx*dx + dz*dz < 100 && !g_invulnerable) {
            if (g_energy) --g_energy;
            if (g_audio_ready) play_one_shot(g_snd_impact, 190u, 0, 190u, SAT_FX16_ONE);
            g_invulnerable = 90u;
            g_speed = -g_speed / 2;
            if (!g_energy) g_finished = 1;
        }
    }
    if ((g_frames % 360u) == 0u && !g_invulnerable && (explorer_hash(g_seed ^ g_frames, g_player.chunk_x, g_player.chunk_z) & 3u) == 0u) {
        if (g_energy) --g_energy;
        if (g_audio_ready) play_one_shot(g_snd_impact, 150u, 0, 180u, SAT_FX16_ONE / 2);
        g_invulnerable = 90u;
        if (!g_energy) g_finished = 1;
    }
    g_target_biome = explorer_biome(g_seed, g_player.chunk_x, g_player.chunk_z);
    if (g_target_biome != g_biome && g_transition == 0u) {
        g_transition = 40u;
        if (g_audio_ready) play_one_shot(g_snd_scanner, 100u, 0, 150u, SAT_FX16_ONE / 2);
    }
    if (g_transition) {
        uint8_t scale;
        if (g_transition > 20u) scale = (uint8_t)((g_transition - 20u) * 31u / 20u);
        else { if (g_transition == 20u) g_biome = g_target_biome; scale = (uint8_t)((20u - g_transition) * 31u / 20u); }
        upload_biome_palettes(g_biome, scale); --g_transition;
    }
    ++g_frames;
}

static void draw_player_and_weather(void) {
    sat_quad3_t craft;
    /* The player's craft is the one thing on screen the player owns, so it is
     * drawn large enough to be read as a craft. It banks with the turn, which
     * on a billboard means shearing the top corners sideways. */
    sat_fx16_t lean = (sat_fx16_t)(g_heading_delta * FX(3));
    /* Hover height: a craft whose base sits at y = 0 is a craft parked on the
     * rocks. Lift the whole quad clear of the ground, and bob it with speed. */
    sat_fx16_t hover = FX(5) + (sat_fx16_t)(g_speed / 3);
    uint8_t v;
    sat_quad3_billboard(&craft, 0, FX(12), FX(1), 0, FX(13), FX(15));
    for (v = 0; v < 4u; ++v) craft.v[v].y += hover;
    /* Banking: shearing only the top corners leans the billboard into the
     * turn, which is as much roll as a flat quad can express. */
    craft.v[0].x += lean;
    craft.v[1].x += lean;
    (void)sat_draw_world_sprite(&g_view_proj, &craft, &g_world_texture, 0, 0);
    if (g_speed > FX(2)) {
        uint8_t i;
        for (i = 0u; i < 5u; ++i) {
            int16_t x = (int16_t)(-10 + (int16_t)i * 5 + (int16_t)((g_frames + i * 3u) & 3u));
            int16_t len = (int16_t)(8 + ((g_frames + i * 7u) & 7u));
            sat_line_cmd_t thrust = {x,72,x, (int16_t)(72 + len), object_color(g_biome,1u),0};
            sat_draw_line(&thrust);
        }
    }
    if (g_transition || ((g_frames / 600u) % 4u) == 1u || ((g_frames / 600u) % 4u) == 3u) {
        uint16_t i; for (i = 0; i < 12u; ++i) {
            int x = (int)((i * 47u + g_frames * 3u) % 320u);
            sat_line_cmd_t l = {(int16_t)(x-160),(int16_t)(-112),(int16_t)(x-170),(int16_t)(-75),SAT_RGB555(20,20,31),0};
            sat_draw_line(&l);
        }
    }
}

static void draw_hud(void) {
    char text[32]; uint8_t i;
    sat_ascii_font_draw_text_screen_indexed8(&g_font, "INFINITE EXPLORER", 8, 4, 8, 0, 0);
    sat_fmt_label_u32("ARTIFACTS ", g_artifacts, text, sizeof(text), 0);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, text, 8, 14, 8, 0, 0);
    sat_fmt_label_u32("DIST ", g_distance, text, sizeof(text), 0);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, text, 176, 14, 8, 0, 0);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, "ENERGY", 8, 204, 8, 0, 0);
    for (i = 0; i < 6u; ++i) sat_draw_rect_screen((int16_t)(64 + i * 12), 205, 9, 6,
        i < g_energy ? object_color(g_biome,1) : SAT_RGB555(4,4,4));
    if (g_paused) sat_ascii_font_draw_text_screen_centered_indexed8(&g_font,"PAUSED",160,104,8,0,0);
    if (g_finished == 1u) sat_ascii_font_draw_text_screen_centered_indexed8(&g_font,"ENERGY LOST - START",160,104,8,0,0);
    if (g_finished == 2u) sat_ascii_font_draw_text_screen_centered_indexed8(&g_font,"EXTRACTED! - START",160,104,8,0,0);
    if (!g_finished && !g_paused) sat_ascii_font_draw_text_screen_indexed8(&g_font,"C SCAN  A FIRE  B BOOST",72,214,8,0,0);
    if (g_scanner && !g_finished) {
        int end = g_signal_dir == -1 ? 122 : (g_signal_dir == 1 ? 198 : 160);
        sat_line_cmd_t l = {0,-76,(int16_t)(end-160),-68,SAT_RGB555(31,31,31),0};
        sat_draw_line(&l);
        sat_ascii_font_draw_text_screen_centered_indexed8(&g_font,
            g_signal_dir == 2 ? "SIGNAL BEHIND" : "SIGNAL",160,30,8,0,0);
    }
    {
        static const char* events[4] = {"CALM","METEORS","DRONES","STORM"};
        sat_ascii_font_draw_text_screen_indexed8(&g_font,events[(g_frames/600u)%4u],248,4,8,0,0);
    }
    if (EXPLORER_HORIZON_REAL_SOURCE == 0u || EXPLORER_REAL_AUDIO_COUNT != EXPLORER_REAL_AUDIO_TOTAL)
        sat_ascii_font_draw_text_screen_indexed8(&g_font,"ASSET FALLBACK",8,26,8,0,0);
}

int main(void) {
    sat_video_config_t cfg = {320,224,1,0};
    sat_example_must(sat_init(&cfg));
    init_layers();
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,2u));
    init_textured_3d();
    sat_example_must(sat_vdp1_set_erase_transparent());
    init_audio();
    reset_game();
    for (;;) {
        sat_pad_state_t pad = {0}; uint16_t params[48]; sat_vdp2_scroll_t sky_scroll;
        int32_t sin_h, cos_h;
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        update_game(&pad);
        update_audio_reactive(&pad);
        sin_h = sat_sin_deg(g_heading); cos_h = sat_cos_deg(g_heading);
        explorer_build_ground_params(g_player.local_x, g_player.local_z, sin_h, cos_h, COEF_BASE_WORD, params);
        sat_example_must(sat_vdp2_vram_write_words(RP_BASE_WORD, params, 48u));
        sky_scroll.x_integer = (uint16_t)(((uint32_t)(g_heading >> 16) * SKY_W / 360u) % SKY_W);
        sky_scroll.x_fraction = 0;
        /* Land the panorama's own horizon line on the Mode-7 horizon. The
         * strip is SKY_H tall and ends at its horizon, so showing its last
         * EXPLORER_HORIZON rows over the top EXPLORER_HORIZON scanlines means
         * scrolling down by the difference. At y=0 the bottom of the strip sat
         * at scanline 128, i.e. 32 rows BELOW the ground's edge, so the part
         * of the panorama with the horizon in it was never visible. */
        sky_scroll.y_integer = (uint16_t)(SKY_H - EXPLORER_HORIZON);
        sky_scroll.y_fraction = 0;
        sat_example_must(sat_vdp2_nbg0_set_scroll(&sky_scroll));
        sat_example_must(sat_vdp2_layers_commit());
        build_render_list(sin_h, cos_h);
        sat_example_must(sat_begin_frame());
        draw_world(); draw_player_and_weather(); draw_hud();
        sat_example_must(sat_end_frame());
    }
}
