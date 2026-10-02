#ifndef SATURN_TESTS_TERRAIN2_TEST_WORLD_HPP
#define SATURN_TESTS_TERRAIN2_TEST_WORLD_HPP

/* Shared by the Terrain2 and Character2 host tests: a profile table and a small
 * tile-grid builder that packs grids into metatiles and binds a map. */

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "saturn/terrain2.h"

#ifndef OK
#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)
#endif

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

static inline uint16_t T(int id, bool fx = false, bool fy = false) {
    return static_cast<uint16_t>(id | (fx ? SAT_TERRAIN2_TILE_FLIP_X : 0) | (fy ? SAT_TERRAIN2_TILE_FLIP_Y : 0));
}

static inline sat_terrain_profile2_t make_profile(const int8_t (&h)[8], uint8_t angle, uint8_t flags = 0,
                                           uint8_t category = 1, uint16_t material = 0) {
    sat_terrain_profile2_t p = {};
    OK(sat_terrain_profile2_from_columns(&p, h, angle, flags, category, material) == SAT_OK);
    OK(sat_terrain_profile2_validate(&p) == SAT_OK);
    return p;
}

static inline std::vector<sat_terrain_profile2_t> make_profiles() {
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

static inline sat_terrain_query2_t Q() { return sat_terrain_query2_default(); }


#endif
