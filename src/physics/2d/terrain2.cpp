#include "saturn/terrain2.h"

#include "src/physics/2d/terrain2_logic.hpp"

namespace t2 = saturn::physics::terrain2;

extern "C" sat_result_t sat_terrain_profile2_from_columns(sat_terrain_profile2_t* out,
    const int8_t heights[8], uint8_t angle, uint8_t flags, uint8_t category, uint16_t material) {
    return t2::profile_from_columns(out, heights, angle, flags, category, material);
}
extern "C" sat_result_t sat_terrain_profile2_validate(const sat_terrain_profile2_t* profile) {
    return t2::profile_validate(profile);
}
extern "C" sat_result_t sat_terrain_map2_requirements(uint16_t profile_count, uint16_t metatile_count,
    uint8_t metatile_shift, uint16_t cols, uint16_t rows, uint8_t layers, uint32_t* out_bytes) {
    return t2::map_requirements(profile_count, metatile_count, metatile_shift, cols, rows, layers, out_bytes);
}
extern "C" sat_result_t sat_terrain_map2_init(sat_terrain_map2_t* map,
    const sat_terrain_profile2_t* profiles, uint16_t profile_count, const uint16_t* metatiles,
    uint16_t metatile_count, uint8_t metatile_shift, uint16_t cols, uint16_t rows) {
    return t2::map_init(map, profiles, profile_count, metatiles, metatile_count, metatile_shift, cols, rows);
}
extern "C" sat_result_t sat_terrain_map2_add_layer(sat_terrain_map2_t* map, const uint16_t* cells) {
    return t2::map_add_layer(map, cells);
}
extern "C" sat_result_t sat_terrain_map2_set_outside(sat_terrain_map2_t* map, sat_terrain_outside2_t policy) {
    return t2::map_set_outside(map, policy);
}
extern "C" sat_result_t sat_terrain_map2_validate(const sat_terrain_map2_t* map) {
    return t2::map_validate(map);
}
extern "C" int sat_terrain2_solid_at(const sat_terrain_map2_t* map, int32_t x, int32_t y,
    const sat_terrain_query2_t* query) {
    return t2::solid_at(map, x, y, query);
}
extern "C" sat_result_t sat_terrain2_sample(const sat_terrain_map2_t* map, int32_t x, int32_t y,
    const sat_terrain_query2_t* query, sat_terrain_hit2_t* out) {
    return t2::sample(map, x, y, query, out);
}
extern "C" sat_result_t sat_terrain2_probe(const sat_terrain_map2_t* map, int32_t x, int32_t y,
    uint8_t dir, int32_t range, const sat_terrain_query2_t* query, sat_terrain_hit2_t* out) {
    return t2::probe(map, x, y, dir, range, query, out);
}
extern "C" sat_result_t sat_terrain2_cast(const sat_terrain_map2_t* map, sat_vec2_t origin,
    sat_vec2_t delta, const sat_terrain_query2_t* query, sat_terrain_hit2_t* out) {
    return t2::cast(map, origin, delta, query, out);
}
