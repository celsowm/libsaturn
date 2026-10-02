#include "view.h"

#include "saturn/color.h"
#include "saturn/render2d.h"
#include "saturn/sprite_clip.h"
#include "saturn/surface.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_layers.h"

#include "art.h"

/* VDP2 VRAM (byte addresses): three pattern-name planes in bank A0, one shared character sheet in
 * bank B0. Every layer is a single 64 x 64 cell ring, which stage_map2 fills as the camera moves. */
#define PLANE_FOREGROUND 0x00000u
#define PLANE_HILLS 0x02000u
#define PLANE_CLOUDS 0x04000u
#define CHAR_BASE 0x40000u

#define LAYER_COUNT 3u

#define FXC(v) ((sat_fx16_t)((v) * 65536.0))

static sat_result_t configure_layer(sat_vdp2_layer_t layer, uint32_t plane, uint8_t priority,
    sat_vdp2_layer_config_t* out) {
    uint32_t i;
    sat_vdp2_layer_config_default(layer, out);
    for (i = 0; i < 4u; ++i) out->plane_address[i] = plane;
    out->char_base_address = CHAR_BASE;
    out->char_bank_mask = SAT_VDP2_BANK_B0;
    out->priority = priority;
    return sat_vdp2_layer_configure(out);
}

sat_result_t hsp_view_init(hsp_view_t* view) {
    static uint16_t cells[HSP_CHAR_COUNT * 16u];
    static uint16_t palettes[48];
    static sat_surface_t sheet;
    static sat_stage_map2_layer_desc_t layers[LAYER_COUNT];
    sat_vdp2_layer_config_t config[LAYER_COUNT];
    sat_stage_map2_tileset_t tileset;
    sat_stage_map2_config_t map_config;
    sat_stage_map2_storage_t storage;
    sat_result_t r;
    uint32_t i;

    hsp_art_cells(cells);
    hsp_art_palettes(palettes);
    r = sat_vdp2_vram_write_words(CHAR_BASE / 2u, cells, HSP_CHAR_COUNT * 16u);
    if (r != SAT_OK) return r;
    r = sat_vdp2_palette_upload(palettes, 48u, 16u);
    if (r != SAT_OK) return r;

    r = configure_layer(SAT_VDP2_NBG0, PLANE_FOREGROUND, 3u, &config[0]);
    if (r != SAT_OK) return r;
    r = configure_layer(SAT_VDP2_NBG1, PLANE_HILLS, 2u, &config[1]);
    if (r != SAT_OK) return r;
    r = configure_layer(SAT_VDP2_NBG2, PLANE_CLOUDS, 1u, &config[2]);
    if (r != SAT_OK) return r;

    for (i = 0; i < LAYER_COUNT; ++i) {
        sat_stage_map2_layer_desc_t* d = &layers[i];
        d->fill_word = 0u;
        d->flags = SAT_STAGE_MAP2_PALETTE_OVERRIDE;
        d->vdp2_layer = (uint8_t)i;
        d->char_bias = 0;
        d->reserved = 0u;
        r = sat_stage_map2_ring_from_layer(&config[i], &d->ring);
        if (r != SAT_OK) return r;
    }
    layers[0].map = stage_map_layer_0;
    layers[0].map_w = STAGE_MAP_LAYER_0_W;
    layers[0].map_h = STAGE_MAP_LAYER_0_H;
    layers[0].outside = SAT_STAGE_MAP2_OUTSIDE_EMPTY;
    layers[0].palette = (uint8_t)HSP_PALETTE_FOREGROUND;
    layers[0].ratio_x = SAT_FX16_ONE;
    layers[0].ratio_y = SAT_FX16_ONE;
    layers[1].map = stage_map_layer_1;
    layers[1].map_w = STAGE_MAP_LAYER_1_W;
    layers[1].map_h = STAGE_MAP_LAYER_1_H;
    layers[1].outside = SAT_STAGE_MAP2_OUTSIDE_WRAP;
    layers[1].palette = (uint8_t)HSP_PALETTE_HILLS;
    layers[1].ratio_x = FXC(0.5);
    layers[1].ratio_y = SAT_FX16_ONE;
    layers[2].map = stage_map_layer_2;
    layers[2].map_w = STAGE_MAP_LAYER_2_W;
    layers[2].map_h = STAGE_MAP_LAYER_2_H;
    layers[2].outside = SAT_STAGE_MAP2_OUTSIDE_WRAP;
    layers[2].palette = (uint8_t)HSP_PALETTE_CLOUDS;
    layers[2].ratio_x = FXC(0.25);
    layers[2].ratio_y = FXC(0.5);

    tileset.cells = stage_map_tileset;
    tileset.metatile_count = STAGE_MAP_METATILE_COUNT;
    tileset.shift = STAGE_MAP_SHIFT;
    tileset.reserved = 0u;
    map_config.tileset = &tileset;
    map_config.layers = layers;
    map_config.layer_count = LAYER_COUNT;
    map_config.margin_cells = 4u;
    map_config.viewport_w = HSP_VIEW_W;
    map_config.viewport_h = HSP_VIEW_H;
    map_config.reserved = 0u;
    storage.words = view->staging_words;
    storage.word_capacity = HSP_STAGING_WORDS;
    storage.runs = view->staging_runs;
    storage.run_capacity = HSP_STAGING_RUNS;
    r = sat_stage_map2_init(&view->map, &map_config, &storage);
    if (r != SAT_OK) return r;

    r = hsp_art_sheet(&sheet);
    if (r != SAT_OK) return r;
    r = sat_texture_create_from_surface(&view->sheet, &sheet, SAT_TEXTURE_PERSISTENT_SOURCE);
    if (r != SAT_OK) return r;
    r = sat_clip_set_prepare_regions(&stage_clip_set, view->sheet);
    if (r != SAT_OK) return r;
    r = sat_ascii_font_init_8x8_indexed8(&view->font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u);
    if (r != SAT_OK) return r;
    view->ready = 1u;
    return SAT_OK;
}

sat_result_t hsp_view_present(hsp_view_t* view) {
    sat_result_t r = sat_vdp2_layers_commit();
    if (r != SAT_OK) return r;
    r = sat_stage_map2_commit_vdp2(&view->map);
    if (r != SAT_OK) return r;
    return sat_stage_map2_apply_scroll_vdp2(&view->map);
}

sat_result_t hsp_view_follow(hsp_view_t* view, const hsp_game_t* game) {
    sat_fx16_t x, y;
    sat_result_t r;
    hsp_game_view_origin(game, &x, &y);
    r = sat_stage_map2_set_view(&view->map, x, y);
    if (r == SAT_ERR_CAPACITY) {
        /* staging is full: send what is there and try again */
        r = sat_stage_map2_commit_vdp2(&view->map);
        if (r != SAT_OK) return r;
        r = sat_stage_map2_set_view(&view->map, x, y);
    }
    return r;
}

static sat_result_t draw_clip(const hsp_view_t* view, const sat_clip_player_t* player, int x, int y, uint8_t flip) {
    return sat_clip_player_draw(player, view->sheet, (int16_t)x, (int16_t)y, flip, 0);
}

static sat_result_t draw_platforms(const hsp_game_t* game) {
    uint32_t i;
    for (i = 0; i < HSP_PLATFORM_COUNT; ++i) {
        sat_box2_t b;
        sat_rect_t r;
        sat_result_t st;
        hsp_game_platform_box(game, i, &b);
        r.x = (int16_t)((b.center.x - b.half.x) >> 16);
        r.y = (int16_t)((b.center.y - b.half.y) >> 16);
        r.width = (uint16_t)(b.half.x >> 15);
        r.height = (uint16_t)(b.half.y >> 15);
        st = sat_fill_rect(&r, sat_color_rgba(120, 90, 50, 255));
        if (st != SAT_OK) return st;
        r.height = 2u;
        st = sat_fill_rect(&r, sat_color_rgba(230, 200, 120, 255));
        if (st != SAT_OK) return st;
    }
    return SAT_OK;
}

sat_result_t hsp_view_draw(hsp_view_t* view, const hsp_game_t* game) {
    sat_result_t r;
    uint32_t i;
    r = sat_render2d_set_camera(&game->camera_out);
    if (r != SAT_OK) return r;

    for (i = 0; i < HSP_MAX_OBJECTS; ++i) {
        const hsp_object_t* o = &game->objects[i];
        if (o->kind == HSP_OBJ_FREE) continue;
        r = draw_clip(view, &o->clip, o->x, o->y, 0u);
        if (r != SAT_OK) return r;
    }
    r = draw_platforms(game);
    if (r != SAT_OK) return r;
    r = draw_clip(view, &game->hero_clip, game->hero.position.x >> 16, game->hero.position.y >> 16,
        game->facing_left ? SAT_CLIP_FLIP_X : 0u);
    if (r != SAT_OK) return r;

    /* the HUD is in screen space: no camera */
    {
        const sat_camera2d_t none = sat_camera2d_default();
        r = sat_render2d_set_camera(&none);
        if (r != SAT_OK) return r;
    }
    r = sat_ascii_font_draw_label_u32(&view->font, "RINGS ", game->stats.rings, 8, 8, 8, 0u, 0u);
    if (r != SAT_OK) return r;
    r = sat_ascii_font_draw_label_u32(&view->font, "SPEED ", (uint32_t)(hsp_game_hero_speed(game) >> 12) * 10u >> 4, 8,
        20, 8, 0u, 0u);
    if (r != SAT_OK) return r;
    if (game->stats.cleared) {
        r = sat_ascii_font_draw_text_screen_centered_indexed8(&view->font, "STAGE CLEAR", HSP_VIEW_W / 2, 100, 8, 0u, 0u);
    }
    return r;
}
