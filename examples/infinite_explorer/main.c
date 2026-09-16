/* infinite_explorer - procedural 360-degree VDP2 world. */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "explorer_logic.h"
#include "infinite_explorer/terrain.h"
#include "infinite_explorer/nebula.h"
#include "infinite_explorer/spacecraft.h"

#define BM_BASE_WORD   0x00000u
#define RP_BASE_WORD   0x10000u
#define COEF_BASE_WORD 0x12000u
#define SKY_W 256u
#define SKY_BAND_H 96u
#define SKY_H (SKY_BAND_H * 3u)
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

static uint8_t g_sky_pixels[SKY_W * SKY_H];
static uint16_t g_map_scratch[SAT_VDP2_NBG0_MAP_CELLS];
static uint16_t g_ground_palette[256], g_sky_palette[256];
static sat_ascii_font_t g_font;
static explorer_pos_t g_player;
static landmark_t g_towers[3], g_portal;
static render_item_t g_render[MAX_RENDER];
static sat_texture_t g_world_texture;
static sat_texture_t g_sky_texture;
static sat_mat4_t g_view_proj;
static uint32_t g_seed = 0x51A7C0DEu, g_frames, g_distance;
static int32_t g_heading, g_speed;
static uint16_t g_render_count, g_drones;
static uint8_t g_energy, g_artifacts, g_biome, g_target_biome;
static uint8_t g_transition, g_invulnerable, g_paused, g_finished;
static uint8_t g_scanner;
static int8_t g_signal_dir;

static uint16_t scale_color(uint16_t c, uint8_t scale) {
    uint16_t r = (uint16_t)((c & 31u) * scale / 31u);
    uint16_t g = (uint16_t)(((c >> 5) & 31u) * scale / 31u);
    uint16_t b = (uint16_t)(((c >> 10) & 31u) * scale / 31u);
    return SAT_BGR555(r, g, b);
}

static void upload_biome_palettes(uint8_t biome, uint8_t scale) {
    uint16_t i;
    (void)biome;
    for (i = 0; i < 256u; ++i) {
        g_ground_palette[i] = scale_color(terrain_palette[i], scale);
        g_sky_palette[i] = scale_color(nebula_palette[i], scale);
    }
    g_ground_palette[0] = 0;
    g_sky_palette[0] = 0;
    sat_example_must(sat_vdp2_palette_upload(g_ground_palette, 256u, 0u));
    sat_example_must(sat_vdp2_palette_upload(g_sky_palette, 256u, 256u));
}

static void generate_visuals(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t y, x, off = BM_BASE_WORD;
    for (y = 0; y < 256u; ++y) {
        for (x = 0; x < 512u; x += 2u) {
            uint8_t a = terrain_pixels[(y & 127u) * 128u + (x & 127u)];
            uint8_t b = terrain_pixels[(y & 127u) * 128u + ((x + 1u) & 127u)];
            vram[off++] = (uint16_t)(((uint16_t)a << 8u) | b);
        }
    }
    for (y = 0; y < SKY_H; ++y) {
        for (x = 0; x < SKY_W; ++x) {
            g_sky_pixels[y * SKY_W + x] = nebula_pixels[(y % 96u) * 128u + (x & 127u)];
        }
    }
}

static void init_textured_3d(void) {
    sat_vec3_t eye = {0, FX(34), FX(-42)}, target = {0, FX(18), FX(90)}, up = {0,FX(1),0};
    sat_mat4_t view, proj;
    sat_example_must(sat_tex_upload_indexed8(&g_world_texture,spacecraft_pixels,64u,64u,spacecraft_palette,3u));
    sat_example_must(sat_tex_upload_indexed8(&g_sky_texture,nebula_pixels,128u,96u,nebula_palette,4u));
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
    generate_visuals();
    upload_biome_palettes(0u, 31u);
    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(g_sky_pixels, SKY_W, SKY_H, 1u, g_map_scratch));
    write_coefficients();
    explorer_build_ground_params(FX(128), FX(128), 0, FX(1), COEF_BASE_WORD, params);
    sat_example_must(sat_vdp2_vram_write_words(RP_BASE_WORD, params, 48u));
    sat_example_must(sat_vdp2_rbg0_mode7_init(&ground));
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_rbg0_set_priority(5u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
}

static int32_t signed_range(uint32_t h, uint32_t min, uint32_t span) {
    int32_t v = (int32_t)(min + h % span);
    return (h & 0x80000000u) ? -v : v;
}

static void reset_game(void) {
    uint8_t i;
    g_seed = g_seed * 1664525u + 1013904223u + g_frames;
    g_player.chunk_x = g_player.chunk_z = 0;
    g_player.local_x = g_player.local_z = FX(128);
    g_heading = g_speed = 0;
    g_frames = g_distance = g_drones = 0;
    g_energy = 6; g_artifacts = 0; g_invulnerable = 0; g_finished = 0; g_paused = 0;
    g_biome = g_target_biome = explorer_biome(g_seed, 0, 0); g_transition = 0;
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
    /* A readable starting landmark composition; the rest of the world remains
     * seed-driven.  It immediately teaches the silhouettes before exploration. */
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

static void draw_world(void) {
    uint16_t i;
    for (i = 0; i < g_render_count; ++i) {
        render_item_t* r = &g_render[i]; sat_quad3_t q;
        int half = r->type==5u ? 30 : (r->type==4u ? 22 : (r->type==2u ? 18 : (r->type==1u ? 12 : 15)));
        int height = r->type==5u ? 46 : (r->type==4u ? 38 : (r->type==2u ? 34 : 26));
        /* The NASA spacecraft is a transparent VDP1 billboard: no more
         * procedural blocks masquerading as scenery. */
        sat_quad3_billboard(&q,r->side,r->depth,FX(1),0,FX(half),FX(height));
        if (r->type == 3u) { uint8_t v; for(v=0;v<4u;++v) q.v[v].y += FX(18); }
        sat_draw_world_sprite(&g_view_proj,&q,&g_world_texture,0,0);
    }
}

static int near_landmark(const landmark_t* l, int radius) {
    int32_t dx = explorer_relative_fx(l->cx, l->lx, g_player.chunk_x, g_player.local_x) >> 16;
    int32_t dz = explorer_relative_fx(l->cz, l->lz, g_player.chunk_z, g_player.local_z) >> 16;
    return dx * dx + dz * dz < radius * radius;
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
    }
}

static void update_game(const sat_pad_state_t* pad) {
    int32_t sin_h, cos_h, thrust = 0;
    uint8_t i;
    if (pad->pressed & SAT_PAD_START) {
        if (g_finished) { reset_game(); return; }
        g_paused ^= 1u;
    }
    if (g_paused || g_finished) return;
    if (pad->held & SAT_PAD_LEFT) g_heading -= FX(2);
    if (pad->held & SAT_PAD_RIGHT) g_heading += FX(2);
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
        if (g_artifacts == 3u) {
            g_portal.cx = g_player.chunk_x + (sin_h >= 0 ? 2 : -2);
            g_portal.cz = g_player.chunk_z + (cos_h >= 0 ? 2 : -2);
            g_portal.lx = g_portal.lz = FX(128); g_portal.active = 1;
        }
    }
    if (g_portal.active && near_landmark(&g_portal, 28) && (pad->held & SAT_PAD_C)) g_finished = 2;
    if ((pad->pressed & SAT_PAD_A) && ((explorer_hash(g_seed, g_player.chunk_x, g_player.chunk_z) & 3u) != 0u)) ++g_drones;
    {
        uint32_t h = explorer_hash(g_seed,g_player.chunk_x,g_player.chunk_z);
        int32_t ox = FX(32 + (h & 191u)), oz = FX(32 + ((h >> 8) & 191u));
        int32_t dx = (ox - g_player.local_x) >> 16, dz = (oz - g_player.local_z) >> 16;
        if (dx*dx + dz*dz < 100 && !g_invulnerable) {
            if (g_energy) --g_energy;
            g_invulnerable = 90u;
            g_speed = -g_speed / 2;
            if (!g_energy) g_finished = 1;
        }
    }
    if ((g_frames % 360u) == 0u && !g_invulnerable && (explorer_hash(g_seed ^ g_frames, g_player.chunk_x, g_player.chunk_z) & 3u) == 0u) {
        if (g_energy) --g_energy;
        g_invulnerable = 90u;
        if (!g_energy) g_finished = 1;
    }
    g_target_biome = explorer_biome(g_seed, g_player.chunk_x, g_player.chunk_z);
    if (g_target_biome != g_biome && g_transition == 0u) g_transition = 40u;
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
    sat_quad3_billboard(&craft,0,FX(6),FX(1),0,FX(11),FX(14));
    sat_draw_world_sprite(&g_view_proj,&craft,&g_world_texture,0,0);
    if (g_transition || ((g_frames / 600u) % 4u) == 1u) {
        uint16_t i; for (i = 0; i < 12u; ++i) {
            int x = (int)((i * 47u + g_frames * 3u) % 320u);
            sat_line_cmd_t l = {(int16_t)(x-160),(int16_t)(-112),(int16_t)(x-170),(int16_t)(-75),SAT_RGB555(20,20,31),0};
            sat_draw_line(&l);
        }
    }
}

static void draw_infinite_sky(void) {
    int i;
    int scroll = (int)(((g_heading >> 16) * 128 / 360 + (int32_t)(g_frames >> 3)) & 127);
    for (i = -2; i < 4; ++i) {
        int x = i * 128 - scroll;
        sat_draw_sprite_scaled_screen(&g_sky_texture,(int16_t)x,24,128u,96u,0u);
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
}

int main(void) {
    sat_video_config_t cfg = {320,224,1,0};
    sat_example_must(sat_init(&cfg));
    init_layers();
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,2u));
    init_textured_3d();
    sat_example_must(sat_vdp1_set_erase_transparent());
    reset_game();
    for (;;) {
        sat_pad_state_t pad = {0}; uint16_t params[48]; sat_vdp2_scroll_t sky_scroll;
        int32_t sin_h, cos_h;
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        update_game(&pad);
        sin_h = sat_sin_deg(g_heading); cos_h = sat_cos_deg(g_heading);
        explorer_build_ground_params(g_player.local_x, g_player.local_z, sin_h, cos_h, COEF_BASE_WORD, params);
        sat_example_must(sat_vdp2_vram_write_words(RP_BASE_WORD, params, 48u));
        sky_scroll.x_integer = (uint16_t)(((g_heading >> 16) * SKY_W / 360) & 255);
        sky_scroll.x_fraction = 0; sky_scroll.y_integer = (uint16_t)(g_biome * SKY_BAND_H); sky_scroll.y_fraction = 0;
        sat_example_must(sat_vdp2_nbg0_set_scroll(&sky_scroll));
        sat_example_must(sat_vdp2_layers_commit());
        build_render_list(sin_h, cos_h);
        sat_example_must(sat_begin_frame());
        draw_infinite_sky(); draw_world(); draw_player_and_weather(); draw_hud();
        sat_example_must(sat_end_frame());
    }
}
