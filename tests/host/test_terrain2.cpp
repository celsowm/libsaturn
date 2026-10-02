#include <cstdio>
#include <cstdlib>
#include <vector>

#include "saturn/physics.h"
#include "saturn/terrain2.h"
#include "src/physics/2d/logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static int absi(int v) { return v < 0 ? -v : v; }
static sat_fx16_t F(int x) { return static_cast<sat_fx16_t>(x * SAT_FX16_ONE); }

/* Profile ids used by every test map. */
enum {
    P_EMPTY = 0,
    P_FULL = 1,
    P_RAMP = 2,    /* 45 degrees rising to the right: heights 1..8 */
    P_SHALLOW = 3, /* rises 3 px over the tile */
    P_STEEP = 4,   /* rises 2 px per px */
    P_STAIR = 5,   /* irregular steps */
    P_ONEWAY = 6,  /* two-pixel platform on the tile's top edge */
    P_SLAB = 7,    /* ceiling slab, three pixels from the top */
    P_WALL = 8,    /* right half solid */
    P_CAT2 = 9,    /* full, category 2 */
    P_TAGGED = 10, /* full, user flag 0x20, material 0x1234 */
    P_COUNT = 11
};

static uint16_t T(int id, bool fx = false, bool fy = false) {
    return static_cast<uint16_t>(id | (fx ? SAT_TERRAIN2_TILE_FLIP_X : 0) | (fy ? SAT_TERRAIN2_TILE_FLIP_Y : 0));
}

static sat_terrain_profile2_t make_profile(const int8_t (&h)[8], uint8_t angle, uint8_t flags = 0,
                                           uint8_t category = 1, uint16_t material = 0) {
    sat_terrain_profile2_t p = {};
    OK(sat_terrain_profile2_from_columns(&p, h, angle, flags, category, material) == SAT_OK);
    OK(sat_terrain_profile2_validate(&p) == SAT_OK);
    return p;
}

static std::vector<sat_terrain_profile2_t> make_profiles() {
    std::vector<sat_terrain_profile2_t> v(P_COUNT);
    const int8_t empty[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const int8_t full[8] = {8, 8, 8, 8, 8, 8, 8, 8};
    const int8_t ramp[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    const int8_t shallow[8] = {0, 0, 1, 1, 2, 2, 3, 3};
    const int8_t steep[8] = {0, 2, 4, 6, 8, 8, 8, 8};
    const int8_t stair[8] = {2, 2, 4, 4, 6, 6, 8, 8};
    const int8_t oneway[8] = {-2, -2, -2, -2, -2, -2, -2, -2};
    const int8_t slab[8] = {-3, -3, -3, -3, -3, -3, -3, -3};
    const int8_t wall[8] = {0, 0, 0, 0, 8, 8, 8, 8};
    v[P_EMPTY] = make_profile(empty, 0);
    v[P_FULL] = make_profile(full, 0, 0, 1, 7);
    v[P_RAMP] = make_profile(ramp, 224);
    v[P_SHALLOW] = make_profile(shallow, 241);
    v[P_STEEP] = make_profile(steep, 211);
    v[P_STAIR] = make_profile(stair, 0);
    v[P_ONEWAY] = make_profile(oneway, 0, SAT_TERRAIN2_ONE_WAY);
    v[P_SLAB] = make_profile(slab, 128);
    v[P_WALL] = make_profile(wall, 192);
    v[P_CAT2] = make_profile(full, 0, 0, 2);
    v[P_TAGGED] = make_profile(full, 0, 0x20, 1, 0x1234);
    return v;
}

/* A test world: tile grids per layer, packed into metatiles of 1 << shift.
 * Tile dimensions must be multiples of the metatile size. */
struct World {
    int wt, ht, shift, layers;
    std::vector<sat_terrain_profile2_t> profiles;
    std::vector<uint16_t> grid[2];
    std::vector<uint16_t> metatiles;
    std::vector<uint16_t> cells[2];
    sat_terrain_map2_t map;

    World(int tiles_w, int tiles_h, int metatile_shift, int layer_count = 1)
        : wt(tiles_w), ht(tiles_h), shift(metatile_shift), layers(layer_count), profiles(make_profiles()), map() {
        for (int l = 0; l < 2; ++l) grid[l].assign(static_cast<size_t>(wt * ht), 0);
    }
    void set(int tx, int ty, uint16_t word, int layer = 0) { grid[layer][static_cast<size_t>(ty * wt + tx)] = word; }
    void fill_rect(int tx0, int ty0, int tx1, int ty1, uint16_t word, int layer = 0) {
        for (int y = ty0; y < ty1; ++y)
            for (int x = tx0; x < tx1; ++x) set(x, y, word, layer);
    }
    void build(sat_terrain_outside2_t outside = SAT_TERRAIN2_OUTSIDE_EMPTY) {
        const int ms = 1 << shift;
        const int cols = wt / ms, rows = ht / ms;
        OK(cols * ms == wt && rows * ms == ht);
        metatiles.clear();
        for (int l = 0; l < layers; ++l) {
            cells[l].assign(static_cast<size_t>(cols * rows), 0);
            for (int cy = 0; cy < rows; ++cy)
                for (int cx = 0; cx < cols; ++cx) {
                    cells[l][static_cast<size_t>(cy * cols + cx)] =
                        static_cast<uint16_t>(l * cols * rows + cy * cols + cx);
                    for (int ty = 0; ty < ms; ++ty)
                        for (int tx = 0; tx < ms; ++tx)
                            metatiles.push_back(grid[l][static_cast<size_t>((cy * ms + ty) * wt + cx * ms + tx)]);
                }
        }
        OK(sat_terrain_map2_init(&map, profiles.data(), static_cast<uint16_t>(profiles.size()), metatiles.data(),
                                 static_cast<uint16_t>(layers * cols * rows), static_cast<uint8_t>(shift),
                                 static_cast<uint16_t>(cols), static_cast<uint16_t>(rows)) == SAT_OK);
        for (int l = 0; l < layers; ++l) OK(sat_terrain_map2_add_layer(&map, cells[l].data()) == SAT_OK);
        OK(sat_terrain_map2_set_outside(&map, outside) == SAT_OK);
        OK(sat_terrain_map2_validate(&map) == SAT_OK);
    }
};

static sat_terrain_query2_t Q() { return sat_terrain_query2_default(); }

/* probe wrapper returning the whole-pixel distance, or kNone */
static const int kNone = 1 << 30;
static int probe(const World& w, int x, int y, int dir, int range, sat_terrain_hit2_t* hit = nullptr,
                 const sat_terrain_query2_t* q = nullptr) {
    sat_terrain_hit2_t local = {};
    sat_terrain_hit2_t& h = hit ? *hit : local;
    const sat_result_t r = sat_terrain2_probe(&w.map, x, y, static_cast<uint8_t>(dir), range, q, &h);
    if (r == SAT_ERR_NOT_FOUND) return kNone;
    OK(r == SAT_OK);
    OK((h.distance & 0xFFFF) == 0);
    return h.distance >> 16;
}

static void flat_ground() {
    World w(8, 6, 1);
    w.fill_rect(0, 4, 8, 6, T(P_FULL));
    w.build();

    sat_terrain_hit2_t h = {};
    OK(probe(w, 10, 20, SAT_TERRAIN2_DIR_DOWN, 40, &h) == 11); /* pixels 21..31 are free */
    OK(h.point.x == (10 << 16) + 0x8000 && h.point.y == F(32));
    OK(h.normal.x == 0 && h.normal.y == -SAT_FX16_ONE);
    OK(h.tangent.x == SAT_FX16_ONE && h.tangent.y == 0);
    OK(h.angle == 0 && h.layer == 0 && h.collider_id == P_FULL);
    OK(h.tile_x == 1 && h.tile_y == 4);
    OK(h.material == 7 && h.flags == 0);

    OK(probe(w, 10, 31, 1, 8) == 0);  /* touching */
    OK(probe(w, 10, 32, 1, 8) == -1); /* one pixel deep */
    OK(probe(w, 10, 35, 1, 8) == -4);
    OK(probe(w, 10, 40, 1, 12) == -9);
    OK(probe(w, 10, 40, 1, 8) == kNone); /* the run is longer than the range */

    /* range counts the pixels examined: a first solid pixel at offset k needs range > k */
    OK(probe(w, 10, 20, 1, 11) == kNone);
    OK(probe(w, 10, 20, 1, 12) == 11);

    OK(sat_terrain2_solid_at(&w.map, 10, 31, nullptr) == 0);
    OK(sat_terrain2_solid_at(&w.map, 10, 32, nullptr) == 1);
    OK(sat_terrain2_solid_at(&w.map, -3, 40, nullptr) == 0); /* outside is empty by default */
}

static void argument_validation() {
    World w(8, 6, 1);
    w.fill_rect(0, 4, 8, 6, T(P_FULL));
    w.build();
    sat_terrain_hit2_t h = {};
    OK(sat_terrain2_probe(nullptr, 0, 0, 1, 8, nullptr, &h) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain2_probe(&w.map, 0, 0, 4, 8, nullptr, &h) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain2_probe(&w.map, 0, 0, 1, 0, nullptr, &h) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain2_probe(&w.map, 0, 0, 1, 256, nullptr, &h) == SAT_ERR_INVALID_ARG);
    sat_terrain_query2_t q = Q();
    q.layer = 1;
    OK(sat_terrain2_probe(&w.map, 0, 0, 1, 8, &q, &h) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain2_sample(&w.map, 0, 0, &q, &h) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain2_solid_at(&w.map, 0, 40, &q) == 0);
    OK(sat_terrain2_probe(&w.map, 10, 20, 1, 40, nullptr, nullptr) == SAT_OK); /* status only */
}

static void legacy_slope_parity() {
    /* The old 45-degree presets and the new profiles agree to half a pixel. */
    namespace ph = saturn::core::physics;
    sat_grid_t grid = {};
    grid.cols = 1;
    grid.rows = 1;
    grid.tile_px = 8;
    for (int mirrored = 0; mirrored < 2; ++mirrored) {
        World w(2, 2, 1);
        w.set(0, 1, T(P_RAMP, mirrored != 0));
        w.build();
        const sat_tile_surface_t surface =
            ph::slope_preset(mirrored ? SAT_TILE_SLOPE_DOWN : SAT_TILE_SLOPE_UP);
        for (int x = 0; x < 8; ++x) {
            sat_fx16_t top = 0;
            OK(ph::surface_top(grid, 0, 0, surface, (x << 16) + 0x8000, top));
            const int legacy_half_px = top >> 15;  /* surface row at the pixel centre, in half pixels */
            const int mine = probe(w, x, 7, 1, 16); /* first solid row relative to the tile top (y = 8) */
            OK(absi(legacy_half_px - 2 * mine) <= 1);
        }
    }
}

static void slopes_report_distance_and_angle() {
    struct Case { int id; uint8_t angle; };
    const Case cases[] = {{P_RAMP, 224}, {P_SHALLOW, 241}, {P_STEEP, 211}, {P_STAIR, 0}};
    for (const Case& c : cases) {
        World w(4, 4, 1);
        w.fill_rect(0, 3, 4, 4, T(P_FULL));
        w.set(1, 2, T(c.id));
        w.build();
        const sat_terrain_profile2_t& p = w.profiles[static_cast<size_t>(c.id)];
        for (int x = 0; x < 8; ++x) {
            sat_terrain_hit2_t h = {};
            const int col = p.column[x];
            /* sensor on the row above the tile (y = 15); an empty column falls through to the floor tile */
            OK(probe(w, 8 + x, 15, 1, 16, &h) == 8 - col);
            if (col == 0) {
                OK(h.collider_id == P_FULL);
            } else {
                OK(h.collider_id == c.id && h.angle == c.angle);
                OK(h.tile_x == 1 && h.tile_y == 2);
                OK(h.normal.x == sat_sin8(c.angle) && h.normal.y == -sat_cos8(c.angle));
                OK(h.tangent.x == sat_cos8(c.angle) && h.tangent.y == sat_sin8(c.angle));
            }
        }
    }
}

static void flips_remap_shape_and_angle() {
    World w(6, 6, 1);
    w.fill_rect(0, 5, 6, 6, T(P_FULL));
    w.set(1, 4, T(P_RAMP));                /* x 8..15,  y 32..39, h = x + 1 from the bottom */
    w.set(3, 4, T(P_RAMP, true, false));   /* x 24..31, h = 8 - x */
    w.set(1, 1, T(P_RAMP, false, true));   /* x 8..15,  y 8..15, hangs from the top, h = x + 1 */
    w.set(3, 1, T(P_RAMP, true, true));    /* x 24..31, hangs from the top, h = 8 - x */
    w.build();

    for (int x = 0; x < 8; ++x) {
        sat_terrain_hit2_t h = {};
        OK(probe(w, 8 + x, 31, 1, 16, &h) == 7 - x);
        OK(h.angle == 224);
        OK(probe(w, 24 + x, 31, 1, 16, &h) == x);
        OK(h.angle == 32);
        OK(h.normal.x > 0 && h.normal.y < 0);
        OK(probe(w, 8 + x, 16, 3, 16, &h) == 7 - x);
        OK(h.angle == static_cast<uint8_t>(128 - 224));
        OK(h.normal.y > 0);
        OK(probe(w, 24 + x, 16, 3, 16, &h) == x);
        OK(h.angle == static_cast<uint8_t>(128 + 224));
        OK(h.collider_id == P_RAMP);
    }
}

static void walls_and_ceilings() {
    World w(8, 6, 1);
    w.set(3, 2, T(P_WALL)); /* x 24..31, y 16..23, solid x 28..31 */
    w.set(5, 4, T(P_SLAB)); /* x 40..47, y 32..39, solid y 32..34 */
    w.build();

    sat_terrain_hit2_t h = {};
    OK(probe(w, 20, 20, SAT_TERRAIN2_DIR_RIGHT, 32, &h) == 7); /* free pixels 21..27 */
    OK(h.angle == 192 && h.normal.x == -SAT_FX16_ONE && h.normal.y == 0);
    OK(h.point.x == F(28) && h.point.y == (20 << 16) + 0x8000);
    OK(h.tile_x == 3 && h.tile_y == 2);
    /* from the right the wall presents its right face, not its stored angle */
    OK(probe(w, 40, 20, SAT_TERRAIN2_DIR_LEFT, 32, &h) == 8); /* free pixels 39..32 */
    OK(h.angle == 64 && h.normal.x == SAT_FX16_ONE && h.normal.y == 0);
    OK(h.point.x == F(32));
    /* straight down onto the wall's top: the stored angle is a wall, so the probe reports a floor face */
    OK(probe(w, 30, 8, SAT_TERRAIN2_DIR_DOWN, 32, &h) == 7);
    OK(h.angle == 0 && h.normal.y == -SAT_FX16_ONE);

    OK(probe(w, 42, 47, SAT_TERRAIN2_DIR_UP, 32, &h) == 12); /* free pixels 46..35, solid at 34 */
    OK(h.angle == 128 && h.normal.y == SAT_FX16_ONE);
    OK(h.point.y == F(35));
    /* seen from above the slab's top is an upward face */
    OK(probe(w, 42, 20, SAT_TERRAIN2_DIR_DOWN, 32, &h) == 11);
    OK(h.angle == 0 && h.normal.y == -SAT_FX16_ONE);

    World f(8, 6, 1); /* Y-flipped, the slab is a floor: solid y 37..39 */
    f.set(5, 4, T(P_SLAB, false, true));
    f.build();
    OK(probe(f, 42, 20, 1, 32, &h) == 16);
    OK(h.angle == 0);
}

/* Brute-force reference for probe(), built on the pixel mask only. */
static int oracle(const World& w, int x, int y, int dir, int range, const sat_terrain_query2_t* q) {
    const int dx = dir == 0 ? 1 : dir == 2 ? -1 : 0;
    const int dy = dir == 1 ? 1 : dir == 3 ? -1 : 0;
    auto solid = [&](int k) { return sat_terrain2_solid_at(&w.map, x + dx * k, y + dy * k, q) != 0; };
    if (!solid(0)) {
        for (int k = 1; k <= range; ++k)
            if (solid(k)) return k - 1;
        return kNone;
    }
    for (int run = 1; run <= range; ++run)
        if (!solid(-run)) return -run;
    return kNone;
}

static void probes_match_a_brute_force_oracle() {
    unsigned seed = 12345u;
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 8;
    };
    const int shapes[] = {P_EMPTY, P_FULL, P_RAMP, P_SHALLOW, P_STEEP, P_STAIR, P_SLAB, P_WALL};
    for (int policy = 0; policy < 3; ++policy) {
        World w(16, 12, 2);
        for (int ty = 0; ty < 12; ++ty)
            for (int tx = 0; tx < 16; ++tx) {
                const int id = shapes[rnd() % 8];
                w.set(tx, ty, T(id, (rnd() & 1) != 0, (rnd() & 1) != 0));
            }
        w.build(static_cast<sat_terrain_outside2_t>(policy));
        long checked = 0;
        for (int y = -12; y < 12 * 8 + 12; ++y) {
            for (int x = -12; x < 16 * 8 + 12; ++x) {
                for (int dir = 0; dir < 4; ++dir) {
                    sat_terrain_hit2_t h = {};
                    const sat_result_t r = sat_terrain2_probe(&w.map, x, y, static_cast<uint8_t>(dir), 40, nullptr, &h);
                    const int want = oracle(w, x, y, dir, 40, nullptr);
                    ++checked;
                    if (want == kNone) {
                        OK(r == SAT_ERR_NOT_FOUND);
                        continue;
                    }
                    OK(r == SAT_OK);
                    OK(h.distance == want * 65536);
                    /* the surface pixel is solid and the one before it, on the sensor side, is free */
                    const int dx = dir == 0 ? 1 : dir == 2 ? -1 : 0;
                    const int dy = dir == 1 ? 1 : dir == 3 ? -1 : 0;
                    const int sx = x + dx * (want + 1), sy = y + dy * (want + 1);
                    OK(sat_terrain2_solid_at(&w.map, sx, sy, nullptr) == 1);
                    OK(sat_terrain2_solid_at(&w.map, sx - dx, sy - dy, nullptr) == 0);
                    if (dx != 0) {
                        OK(h.point.x == (dx > 0 ? sx : sx + 1) * 65536 && h.point.y == sy * 65536 + 0x8000);
                    } else {
                        OK(h.point.y == (dy > 0 ? sy : sy + 1) * 65536 && h.point.x == sx * 65536 + 0x8000);
                    }
                    /* normal and tangent are perpendicular unit vectors */
                    OK(absi(sat_vec2_dot(h.normal, h.tangent)) < 4);
                    OK(absi(sat_vec2_length(h.normal) - SAT_FX16_ONE) < 4);
                    /* a hit inside the map names the tile that holds the surface pixel */
                    if (sx >= 0 && sx < 128 && sy >= 0 && sy < 96) OK(h.tile_x == (sx >> 3) && h.tile_y == (sy >> 3));
                }
            }
        }
        OK(checked == 152L * 120L * 4L);
    }
}

static void horizontal_and_vertical_probes_agree() {
    /* Staircases, flipped, read through rows (horizontal) and columns (vertical): the oracle
     * is built from the column mask alone, so any row/column disagreement shows here. */
    World w(4, 4, 1);
    w.set(1, 1, T(P_STAIR));
    w.set(2, 1, T(P_STAIR, true, true));
    w.set(1, 2, T(P_STEEP, true, false));
    w.set(2, 2, T(P_SHALLOW, false, true));
    w.build();
    for (int y = 4; y < 28; ++y)
        for (int dir = 0; dir < 4; ++dir)
            for (int x = 4; x < 28; ++x) OK(probe(w, x, y, dir, 20) == oracle(w, x, y, dir, 20, nullptr));
}

static void adjacent_profiles_are_continuous() {
    /* floor, ramp, plateau; the plateau starts on a metatile boundary (metatiles are 4 tiles wide) */
    World m(12, 8, 2);
    m.fill_rect(0, 4, 4, 8, T(P_FULL)); /* floor, top row y = 32 */
    m.set(3, 3, T(P_RAMP));              /* x 24..31, y 24..31 */
    m.fill_rect(4, 3, 12, 8, T(P_FULL)); /* plateau, top row y = 24 */
    m.build();
    int prev = -1;
    for (int x = 0; x < 12 * 8; ++x) {
        const int top = probe(m, x, 0, 1, 255) + 1; /* first solid row */
        if (prev >= 0) OK(absi(top - prev) <= 1);
        prev = top;
    }
    OK(probe(m, 5, 0, 1, 255) + 1 == 32);
    OK(probe(m, 80, 0, 1, 255) + 1 == 24);
}

static void layers_and_filters() {
    World w(8, 6, 1, 2);
    w.fill_rect(0, 4, 8, 6, T(P_FULL), 0); /* floor top row y = 32 */
    w.fill_rect(0, 2, 8, 6, T(P_FULL), 1); /* higher floor, top row y = 16 */
    w.set(2, 3, T(P_CAT2), 0);              /* x 16..23, y 24..31 */
    w.set(4, 3, T(P_TAGGED), 0);            /* x 32..39 */
    w.build();

    sat_terrain_query2_t q = Q();
    sat_terrain_hit2_t h = {};
    OK(probe(w, 6, 0, 1, 255, &h, &q) == 31);
    q.layer = 1;
    OK(probe(w, 6, 0, 1, 255, &h, &q) == 15);
    OK(h.layer == 1);

    q = Q();
    q.category_mask = 1;
    OK(probe(w, 20, 0, 1, 255, &h, &q) == 31); /* the category-2 tile is skipped */
    q.category_mask = 2;
    OK(probe(w, 20, 0, 1, 255, &h, &q) == 23 && h.collider_id == P_CAT2);
    q.category_mask = 0xFF;
    OK(probe(w, 20, 0, 1, 255, &h, &q) == 23);
    q.category_mask = 0;
    OK(probe(w, 20, 0, 1, 255, &h, &q) == kNone);

    q = Q();
    OK(probe(w, 36, 0, 1, 255, &h, &q) == 23 && h.flags == 0x20 && h.material == 0x1234);
    OK(h.collider_id == P_TAGGED && h.tile_x == 4 && h.tile_y == 3);
    q.ignore_flags = 0x20;
    OK(probe(w, 36, 0, 1, 255, &h, &q) == 31 && h.collider_id == P_FULL);
}

static void one_way_follows_the_surface_normal() {
    World w(6, 6, 1);
    w.set(1, 3, T(P_ONEWAY));              /* x 8..15: rows 24, 25 */
    w.set(3, 3, T(P_ONEWAY, false, true)); /* x 24..31: rows 30, 31, facing down */
    w.build();

    /* a platform blocks only a probe travelling against its normal */
    OK(probe(w, 12, 16, SAT_TERRAIN2_DIR_DOWN, 32) == 7);     /* lands on row 24 */
    OK(probe(w, 12, 40, SAT_TERRAIN2_DIR_UP, 32) == kNone);   /* passes through from below */
    OK(probe(w, 4, 25, SAT_TERRAIN2_DIR_RIGHT, 32) == kNone); /* side probes pass */
    OK(probe(w, 20, 25, SAT_TERRAIN2_DIR_LEFT, 32) == kNone);
    /* a sensor already inside the platform sees support when travelling down, nothing when going up */
    OK(probe(w, 12, 25, SAT_TERRAIN2_DIR_DOWN, 8) == -2);
    OK(probe(w, 12, 25, SAT_TERRAIN2_DIR_UP, 8) == kNone);
    OK(sat_terrain2_solid_at(&w.map, 12, 24, nullptr) == 0); /* a point query has no direction */

    /* flipped in Y the surface faces down: it catches upward probes only */
    sat_terrain_hit2_t h = {};
    OK(probe(w, 28, 47, SAT_TERRAIN2_DIR_UP, 32, &h) == 15);
    OK(h.angle == 128 && h.flags == SAT_TERRAIN2_ONE_WAY);
    OK(probe(w, 28, 16, SAT_TERRAIN2_DIR_DOWN, 32) == kNone);
}

static void cast_finds_the_first_solid_pixel() {
    World w(8, 6, 1);
    w.fill_rect(0, 4, 8, 6, T(P_FULL)); /* floor, top row y = 32 */
    w.set(5, 2, T(P_WALL));              /* x 40..47, solid x 44..47, y 16..23 */
    w.build();

    sat_terrain_hit2_t h = {};
    OK(sat_terrain2_cast(&w.map, {F(10), F(20)}, {F(50), 0}, nullptr, &h) == SAT_OK);
    OK(h.point.x == F(44) && h.point.y == F(20));
    OK(h.distance == static_cast<sat_fx16_t>((34ll << 16) / 50));
    OK(h.angle == 192 && h.collider_id == P_WALL);
    /* a diagonal cast lands on the floor */
    OK(sat_terrain2_cast(&w.map, {F(4), F(4)}, {F(40), F(40)}, nullptr, &h) == SAT_OK);
    OK((h.point.y >> 16) == 32 && (h.point.x >> 16) == 32);
    OK(h.angle == 0 && h.normal.y == -SAT_FX16_ONE);
    /* starting inside solid reports distance 0 */
    OK(sat_terrain2_cast(&w.map, {F(4), F(40)}, {F(10), 0}, nullptr, &h) == SAT_OK && h.distance == 0);
    /* nothing in the way, and an over-long cast */
    OK(sat_terrain2_cast(&w.map, {F(4), F(4)}, {F(20), 0}, nullptr, &h) == SAT_ERR_NOT_FOUND);
    OK(sat_terrain2_cast(&w.map, {F(4), F(4)}, {F(600), 0}, nullptr, &h) == SAT_ERR_INVALID_ARG);
    /* one-way tiles follow the cast direction */
    World o(4, 4, 1);
    o.set(1, 2, T(P_ONEWAY)); /* x 8..15, rows 16, 17 */
    o.build();
    OK(sat_terrain2_cast(&o.map, {F(12), F(0)}, {0, F(30)}, nullptr, &h) == SAT_OK && (h.point.y >> 16) == 16);
    OK(sat_terrain2_cast(&o.map, {F(12), F(30)}, {0, F(-30)}, nullptr, &h) == SAT_ERR_NOT_FOUND);
}

static void outside_policies() {
    sat_terrain_hit2_t h = {};
    {
        World w(4, 4, 1);
        w.fill_rect(0, 3, 4, 4, T(P_FULL)); /* floor, top row y = 24 */
        w.build(SAT_TERRAIN2_OUTSIDE_EMPTY);
        OK(probe(w, -4, 0, 1, 255) == kNone);
        OK(probe(w, 40, 0, 1, 255) == kNone);
        OK(probe(w, 8, 20, 1, 255) == 3);
    }
    {
        World w(4, 4, 1);
        w.build(SAT_TERRAIN2_OUTSIDE_SOLID);
        OK(probe(w, 8, 10, 1, 255, &h) == 21); /* map bottom at y = 32 */
        OK(h.collider_id == 0xFFFF && h.angle == 0 && h.material == 0 && h.flags == 0);
        OK(probe(w, 8, 10, 0, 255, &h) == 23);
        OK(h.angle == 192);
        OK(probe(w, 35, 10, 0, 255) == -4); /* inside the right-hand solid: back out to x = 31 */
        OK(probe(w, -6, 10, 0, 255) == kNone); /* solid all the way left */
    }
    {
        World w(4, 4, 1);
        w.fill_rect(0, 3, 4, 4, T(P_FULL));
        w.build(SAT_TERRAIN2_OUTSIDE_CLAMP);
        OK(probe(w, -20, 0, 1, 255) == 23);
        OK(probe(w, 100, 0, 1, 255) == 23);
        OK(probe(w, 8, 60, 1, 255) == -37); /* the bottom row repeats downwards */
    }
}

static void sample_reports_the_tile() {
    World w(4, 4, 1);
    w.set(1, 1, T(P_RAMP));
    w.set(2, 1, T(P_ONEWAY));
    w.build();
    sat_terrain_hit2_t h = {};
    OK(sat_terrain2_sample(&w.map, 9, 9, nullptr, &h) == SAT_OK);
    OK(h.angle == 224 && h.collider_id == P_RAMP && h.distance == 0 && h.tile_x == 1 && h.tile_y == 1);
    OK(h.point.x == (9 << 16) + 0x8000);
    OK(sat_terrain2_sample(&w.map, 1, 1, nullptr, &h) == SAT_ERR_NOT_FOUND); /* empty tile */
    OK(sat_terrain2_sample(&w.map, 17, 9, nullptr, &h) == SAT_OK);          /* one-way is present */
    OK(sat_terrain2_sample(&w.map, -9, 9, nullptr, &h) == SAT_ERR_NOT_FOUND);
    sat_terrain_query2_t q = Q();
    q.category_mask = 2;
    OK(sat_terrain2_sample(&w.map, 9, 9, &q, &h) == SAT_ERR_NOT_FOUND);
}

static void profile_and_map_construction() {
    sat_terrain_profile2_t p = {};
    const int8_t bad_range[8] = {9, 0, 0, 0, 0, 0, 0, 0};
    const int8_t valley[8] = {8, 6, 4, 2, 2, 4, 6, 8};
    const int8_t hill[8] = {-1, -2, -3, -4, -4, -3, -2, -1};
    OK(sat_terrain_profile2_from_columns(&p, bad_range, 0, 0, 1, 0) == SAT_ERR_INVALID_ARG);
    /* rows are single runs anchored to a side, so valleys and hills must be split across tiles */
    OK(sat_terrain_profile2_from_columns(&p, valley, 0, 0, 1, 0) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain_profile2_from_columns(&p, hill, 0, 0, 1, 0) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain_profile2_from_columns(nullptr, hill, 0, 0, 1, 0) == SAT_ERR_INVALID_ARG);

    const int8_t good[8] = {0, 1, 2, 3, 4, 5, 6, 8};
    OK(sat_terrain_profile2_from_columns(&p, good, 3, 0x11, 4, 99) == SAT_OK);
    OK(p.angle == 3 && p.flags == 0x11 && p.category == 4 && p.material == 99);
    OK(sat_terrain_profile2_validate(&p) == SAT_OK);
    sat_terrain_profile2_t broken = p;
    broken.row[7] = static_cast<int8_t>(broken.row[7] - 1);
    OK(sat_terrain_profile2_validate(&broken) == SAT_ERR_INVALID_ARG);
    broken = p;
    broken.column[0] = 9;
    OK(sat_terrain_profile2_validate(&broken) == SAT_ERR_INVALID_ARG);
    OK(sizeof(sat_terrain_profile2_t) == 22);

    World w(8, 8, 2);
    w.build();
    w.metatiles[3] = 500; /* no such profile */
    OK(sat_terrain_map2_validate(&w.map) == SAT_ERR_INVALID_ARG);
    w.metatiles[3] = 0;
    OK(sat_terrain_map2_validate(&w.map) == SAT_OK);
    w.cells[0][1] = 77; /* no such metatile */
    OK(sat_terrain_map2_validate(&w.map) == SAT_ERR_INVALID_ARG);

    OK(sat_terrain_map2_add_layer(&w.map, w.cells[0].data()) == SAT_OK);
    OK(sat_terrain_map2_add_layer(&w.map, w.cells[0].data()) == SAT_OK);
    OK(sat_terrain_map2_add_layer(&w.map, w.cells[0].data()) == SAT_OK);
    OK(sat_terrain_map2_add_layer(&w.map, w.cells[0].data()) == SAT_ERR_CAPACITY);

    sat_terrain_map2_t m = {};
    OK(sat_terrain_map2_init(&m, nullptr, 0, nullptr, 0, 6, 1, 1) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain_map2_init(&m, nullptr, 0, nullptr, 0, 2, 0, 1) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain_map2_init(&m, nullptr, 3, nullptr, 0, 2, 1, 1) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain_map2_set_outside(&m, static_cast<sat_terrain_outside2_t>(7)) == SAT_ERR_INVALID_ARG);

    uint32_t bytes = 0;
    OK(sat_terrain_map2_requirements(11, 4, 2, 2, 2, 2, &bytes) == SAT_OK);
    OK(bytes == 11u * 22u + 4u * 16u * 2u + 2u * 2u * 2u * 2u);
    OK(sat_terrain_map2_requirements(11, 4, 6, 2, 2, 2, &bytes) == SAT_ERR_INVALID_ARG);
    OK(sat_terrain_map2_requirements(11, 4, 2, 2, 2, 5, &bytes) == SAT_ERR_INVALID_ARG);
}

int main() {
    flat_ground();
    argument_validation();
    legacy_slope_parity();
    slopes_report_distance_and_angle();
    flips_remap_shape_and_angle();
    walls_and_ceilings();
    probes_match_a_brute_force_oracle();
    horizontal_and_vertical_probes_agree();
    adjacent_profiles_are_continuous();
    layers_and_filters();
    one_way_follows_the_surface_normal();
    cast_finds_the_first_solid_pixel();
    outside_policies();
    sample_reports_the_tile();
    profile_and_map_construction();
    std::puts("PASS: test_terrain2.cpp");
    return 0;
}
