#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "fixtures/synth.c" /* the committed output of tools/stage2d_tool.py for tests/fixtures/stage2d/synthetic_stage.json */

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

/* The offline tools write C arrays; this test feeds them to the real runtime validators and
 * queries, so a disagreement between the tool and the runtime shows up here and not in a game. */

static void test_profiles() {
    for (unsigned i = 0; i < SYNTH_PROFILE_COUNT; ++i) {
        const sat_terrain_profile2_t& p = synth_profiles[i];
        OK(sat_terrain_profile2_validate(&p) == SAT_OK);
        /* the tool derives the row table the same way the runtime does */
        sat_terrain_profile2_t again;
        OK(sat_terrain_profile2_from_columns(&again, p.column, p.angle, p.flags, p.category, p.material) == SAT_OK);
        for (int k = 0; k < 8; ++k) OK(again.row[k] == p.row[k]);
    }
    OK(synth_profiles[SYNTH_PROFILE_RAMP].angle == 224 && synth_profiles[SYNTH_PROFILE_RAMP_FLIP_X].angle == 32);
    OK(synth_profiles[SYNTH_PROFILE_PLAT].flags == SAT_TERRAIN2_ONE_WAY);
    OK(synth_profiles[SYNTH_PROFILE_HALF_FLIP_Y].column[0] == -4);
    /* a ramp turned a quarter turn: a wall whose travel direction points the opposite way to a floor's */
    const sat_terrain_profile2_t& wall = synth_profiles[SYNTH_PROFILE_RAMP_ROT90];
    OK(wall.angle == (224 + 64) % 256);
}

struct Terrain {
    sat_terrain_map2_t map;
    Terrain() {
        OK(sat_terrain_map2_init(&map, synth_profiles, SYNTH_PROFILE_COUNT, synth_terrain_metatiles, SYNTH_TERRAIN_METATILE_COUNT,
                                 SYNTH_TERRAIN_METATILE_SHIFT, SYNTH_TERRAIN_COLS, SYNTH_TERRAIN_ROWS) == SAT_OK);
        OK(sat_terrain_map2_add_layer(&map, synth_terrain_layer_0) == SAT_OK);
        OK(sat_terrain_map2_add_layer(&map, synth_terrain_layer_1) == SAT_OK);
        OK(sat_terrain_map2_set_outside(&map, SYNTH_TERRAIN_OUTSIDE) == SAT_OK);
    }
};

static int probe_down(const sat_terrain_map2_t* map, int x, int y, const sat_terrain_query2_t* q = nullptr) {
    sat_terrain_hit2_t h;
    const sat_result_t r = sat_terrain2_probe(map, x, y, SAT_TERRAIN2_DIR_DOWN, 64, q, &h);
    OK(r == SAT_OK);
    OK((h.distance & 0xFFFF) == 0);
    return h.distance >> 16;
}

static void test_terrain() {
    Terrain t;
    OK(sat_terrain_map2_validate(&t.map) == SAT_OK);
    OK(t.map.width_px == 64 && t.map.height_px == 32 && t.map.layer_count == 2);

    uint32_t bytes = 0;
    OK(sat_terrain_map2_requirements(SYNTH_PROFILE_COUNT, SYNTH_TERRAIN_METATILE_COUNT, SYNTH_TERRAIN_METATILE_SHIFT,
                                     SYNTH_TERRAIN_COLS, SYNTH_TERRAIN_ROWS, SYNTH_TERRAIN_LAYER_COUNT, &bytes) == SAT_OK);
    OK(bytes == 176 + 80); /* what the tool's size report charges */

    /* layer 0: a block floor from y = 24, a ramp on tile (2, 2), a one-way platform on tile (4, 2) */
    OK(sat_terrain2_solid_at(&t.map, 4, 26, nullptr) == 1);
    OK(sat_terrain2_solid_at(&t.map, 4, 10, nullptr) == 0);
    OK(sat_terrain2_solid_at(&t.map, 23, 16, nullptr) == 1);  /* the tall end of the ramp */
    OK(sat_terrain2_solid_at(&t.map, 16, 22, nullptr) == 0 && sat_terrain2_solid_at(&t.map, 16, 23, nullptr) == 1);
    OK(sat_terrain2_solid_at(&t.map, -5, 5, nullptr) == 1);   /* outside is solid */
    OK(probe_down(&t.map, 4, 0) == 23);
    OK(probe_down(&t.map, 35, 0) == 21); /* the platform's top, from above */
    sat_terrain_query2_t ignore = sat_terrain_query2_default();
    ignore.ignore_flags = SAT_TERRAIN2_ONE_WAY;
    OK(probe_down(&t.map, 35, 0, &ignore) == 23);

    sat_terrain_hit2_t hit;
    OK(sat_terrain2_sample(&t.map, 20, 18, nullptr, &hit) == SAT_OK);
    OK(hit.collider_id == SYNTH_PROFILE_RAMP && hit.material == 3 && hit.angle == 224);

    /* layer 1: a flipped ramp on tile (3, 2) and a half-height floor from y = 28 */
    sat_terrain_query2_t second = sat_terrain_query2_default();
    second.layer = 1;
    OK(probe_down(&t.map, 4, 0, &second) == 27);
    OK(sat_terrain2_solid_at(&t.map, 24, 16, &second) == 1 && sat_terrain2_solid_at(&t.map, 31, 22, &second) == 0);
    OK(sat_terrain2_sample(&t.map, 28, 18, &second, &hit) == SAT_OK);
    OK(hit.collider_id == SYNTH_PROFILE_RAMP && hit.angle == 32 && (hit.flags & SAT_TERRAIN2_ONE_WAY) == 0);
}

struct Spawner {
    std::vector<uint32_t> order;
    static sat_entity_activate_result_t activate(void* user, uint32_t index, const sat_entity_desc2_t*) {
        static_cast<Spawner*>(user)->order.push_back(index);
        return SAT_ENTITY_ACTIVATED;
    }
};

static void test_entities() {
    OK(sat_entity_index2_validate(&synth_entity_index) == SAT_OK);
    OK(sat_entity_index2_bytes(SYNTH_ENTITY_COUNT, SYNTH_ENTITY_REGION_COUNT) == 96); /* the tool's accounting */
    OK(synth_entity_index.origin_x == 0 && synth_entity_index.origin_y == 0 && synth_entity_index.region_cols == 5);

    uint32_t state[2];
    OK(sat_entity_stream2_state_words(SYNTH_ENTITY_COUNT) == 2);
    Spawner spawn;
    sat_entity_stream2_t stream;
    sat_entity_stream2_config_t cfg = {};
    cfg.index = &synth_entity_index;
    cfg.state = state;
    cfg.state_words = 2;
    cfg.activate = Spawner::activate;
    cfg.user = &spawn;
    OK(sat_entity_stream2_init(&stream, &cfg) == SAT_OK);

    /* world box [0, 64) x [0, 32): the four entities the spec put there, in descriptor order */
    sat_box2_t box = {{32 << 16, 16 << 16}, {32 << 16, 16 << 16}};
    OK(sat_entity_stream2_update(&stream, &box, nullptr) == SAT_OK);
    OK(spawn.order.size() == 4);
    for (size_t i = 1; i < spawn.order.size(); ++i) OK(spawn.order[i - 1] < spawn.order[i]);
    int kind1 = 0;
    for (uint32_t i : spawn.order) {
        const sat_entity_desc2_t& d = synth_entity_descs[i];
        OK(d.x < 64 && d.y < 32);
        kind1 += d.kind == 1;
    }
    OK(kind1 == 3); /* (40,24) (41,25) (63,31) */

    /* the whole stage */
    box = {{80 << 16, 50 << 16}, {400 << 16, 400 << 16}};
    OK(sat_entity_stream2_update(&stream, &box, nullptr) == SAT_OK);
    OK(sat_entity_stream2_active_count(&stream) == SYNTH_ENTITY_COUNT);
}

struct Events {
    std::vector<uint16_t> ids;
    static void fn(void* user, uint16_t ev, uint16_t, uint16_t) { static_cast<Events*>(user)->ids.push_back(ev); }
};

static void test_clips() {
    OK(sat_clip_set_validate(&synth_clip_set) == SAT_OK);
    OK(sat_clip_set_region_count(&synth_clip_set) == SYNTH_CLIP_REGION_COUNT && SYNTH_CLIP_REGION_COUNT == 10);

    sat_clip_player_t p;
    Events ev;
    OK(sat_clip_player_init(&p, &synth_clip_set) == SAT_OK);
    OK(sat_clip_player_play(&p, SYNTH_CLIP_RUN, SAT_CLIP_SWITCH_RESTART, Events::fn, &ev) == SAT_OK);
    OK(ev.ids.size() == 1 && ev.ids[0] == 10);
    for (int i = 0; i < 6; ++i) OK(sat_clip_player_step(&p, Events::fn, &ev, nullptr) == SAT_OK);
    OK(sat_clip_player_frame_index(&p) == 1 && p.loops == 1); /* the loop returns to loop_start = 1 */
    OK(ev.ids.size() == 1);                                   /* frame 0's event is not repeated */

    sat_clip_shape_t s;
    OK(sat_clip_frame_find_shape(&synth_clip_set, sat_clip_player_frame(&p), 2, 1, &s) && s.x == 4 && s.w == 6);
    OK(sat_clip_frame_find_shape(&synth_clip_set, sat_clip_player_frame(&p), 1, 0xFF, &s) && s.h == 14);

    OK(sat_clip_player_play(&p, SYNTH_CLIP_JUMP, SAT_CLIP_SWITCH_RESTART, nullptr, nullptr) == SAT_OK);
    OK(sat_clip_player_advance(&p, 100 << 16, nullptr, nullptr, nullptr) == SAT_OK && sat_clip_player_is_finished(&p));
    OK(sat_clip_frame_find_shape(&synth_clip_set, sat_clip_player_frame(&p), 3, 0xFF, &s) && s.w == 0 && s.x == 6);

    sat_rect_t r;
    OK(sat_clip_frame_dest(sat_clip_player_frame(&p), 100, 100, 0, &r) == SAT_OK);
    OK(r.x == 92 && r.y == 92 && r.width == 16); /* pivot "center" is (8, 8) */
}

static void test_paths() {
    const sat_vec2_t* a = synth_path_arc_points;
    sat_path2_t arc;
    OK(sat_path2_init_cubic(&arc, a[0], a[1], a[2], a[3]) == SAT_OK);
    OK(sat_path2_attach_table(&arc, synth_path_arc_table, SYNTH_PATH_ARC_TABLE_ENTRIES) == SAT_OK);
    OK(sat_path2_length(&arc) == synth_path_arc_table[SYNTH_PATH_ARC_TABLE_ENTRIES - 1]);
    /* entry 32 of 65 is the curve parameter 0.5, the point (40, 20) */
    sat_path2_sample_t s;
    OK(sat_path2_sample(&arc, synth_path_arc_table[32], &s) == SAT_OK);
    OK(std::fabs(s.position.x / 65536.0 - 40.0) < 0.05 && std::fabs(s.position.y / 65536.0 - 20.0) < 0.05);

    const sat_vec2_t* d = synth_path_dip_points;
    sat_path2_t dip;
    OK(sat_path2_init_quadratic(&dip, d[0], d[1], d[2]) == SAT_OK);
    OK(sat_path2_attach_table(&dip, synth_path_dip_table, SYNTH_PATH_DIP_TABLE_ENTRIES) == SAT_OK);
    OK(sat_path2_sample(&dip, synth_path_dip_table[16], &s) == SAT_OK);
    OK(std::fabs(s.position.x / 65536.0 - 30.0) < 0.05 && std::fabs(s.position.y / 65536.0 - 25.0) < 0.05);

    /* the offline table is at least as accurate as the one the runtime builds from coarse chords */
    sat_path2_t runtime;
    std::vector<sat_fx16_t> table(SYNTH_PATH_ARC_TABLE_ENTRIES);
    OK(sat_path2_init_cubic(&runtime, a[0], a[1], a[2], a[3]) == SAT_OK);
    OK(sat_path2_build_table(&runtime, table.data(), SYNTH_PATH_ARC_TABLE_ENTRIES) == SAT_OK);
    const double offline = sat_path2_length(&arc) / 65536.0, coarse = sat_path2_length(&runtime) / 65536.0;
    OK(std::fabs(offline - coarse) < 0.5 && offline >= coarse - 1e-3);
}

static void test_map_tileset() {
    /* the stage-map tables: metatile 0 is the all-fill one, layers index the shared table */
    for (unsigned i = 0; i < 4; ++i) OK(synth_map_tileset[i] == 0);
    for (unsigned y = 0; y < SYNTH_MAP_LAYER_0_H; ++y)
        for (unsigned x = 0; x < SYNTH_MAP_LAYER_0_W; ++x) OK(synth_map_layer_0[y * SYNTH_MAP_LAYER_0_W + x] < SYNTH_MAP_METATILE_COUNT);
    /* cell (2, 2) of layer 0 is word 1, in the metatile at map (1, 1) */
    const unsigned m = synth_map_layer_0[1 * SYNTH_MAP_LAYER_0_W + 1];
    OK(synth_map_tileset[m * 4 + 0] == 1 && synth_map_tileset[m * 4 + 1] == 2);
    OK(synth_map_tileset[m * 4 + 2] == 3 && synth_map_tileset[m * 4 + 3] == 4);
}

int main() {
    test_profiles();
    test_terrain();
    test_entities();
    test_clips();
    test_paths();
    test_map_tileset();
    std::puts("test_stage2d_generated ok");
    return 0;
}
