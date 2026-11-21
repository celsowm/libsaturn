#include <cassert>
#include "saturn/water_regions.h"

namespace {

constexpr sat_fx16_t one = SAT_FX16_ONE;

sat_water_volume_t box(int x0, int x1, int z0, int z1, int bottom, int surface) {
    sat_water_volume_t v{};
    v.min_x = x0 * one; v.max_x = x1 * one; v.min_z = z0 * one; v.max_z = z1 * one;
    v.bottom = bottom * one; v.surface = surface * one;
    v.buoyancy = one / 20; v.drag = one / 5;
    return v;
}

void init_validation() {
    sat_water_volume_t v[2] = {box(0, 4, 0, 4, -3, 0), box(4, 8, 0, 4, -3, 0)};
    sat_water_regions_t r;
    assert(sat_water_regions_init(nullptr, v, 2) == SAT_ERR_INVALID_ARG);
    assert(sat_water_regions_init(&r, nullptr, 2) == SAT_ERR_INVALID_ARG);
    assert(sat_water_regions_init(&r, nullptr, 0) == SAT_OK && r.count == 0);
    v[1].surface = v[1].bottom;
    assert(sat_water_regions_init(&r, v, 2) == SAT_ERR_INVALID_ARG);
    v[1].surface = 0;
    assert(sat_water_regions_init(&r, v, 2) == SAT_OK && r.count == 2);
    assert(sat_water_regions_set_surface(&r, 1, -3 * one) == SAT_ERR_INVALID_ARG);
    assert(sat_water_regions_set_surface(&r, 2, one) == SAT_ERR_INVALID_ARG);
    assert(sat_water_regions_set_surface(&r, 1, -one) == SAT_OK && v[1].surface == -one);
}

void find_picks_the_highest_surface() {
    sat_water_volume_t v[3] = {box(0, 8, 0, 8, -4, -1), box(2, 6, 2, 6, -6, 1), box(20, 24, 0, 4, -3, 0)};
    sat_water_regions_t r;
    assert(sat_water_regions_init(&r, v, 3) == SAT_OK);
    uint16_t index = 7;
    sat_vec3_t p = {one, 2 * one, one};
    assert(sat_water_regions_find(&r, &p, &index) == SAT_OK && index == 0);
    p = {3 * one, 0, 3 * one};  // overlap: region 1 has the higher surface
    assert(sat_water_regions_find(&r, &p, &index) == SAT_OK && index == 1);
    p = {3 * one, -5 * one, 3 * one};  // below region 0's bottom
    assert(sat_water_regions_find(&r, &p, &index) == SAT_OK && index == 1);
    p = {3 * one, -7 * one, 3 * one};
    assert(sat_water_regions_find(&r, &p, &index) == SAT_OK && index == SAT_WATER_REGION_NONE);
    p = {12 * one, 0, 1 * one};
    assert(sat_water_regions_find(&r, &p, &index) == SAT_OK && index == SAT_WATER_REGION_NONE);
    // Bounds are inclusive; a moving surface changes nothing about "over".
    p = {8 * one, 0, 8 * one};
    assert(sat_water_regions_find(&r, &p, &index) == SAT_OK && index == 0);
    assert(sat_water_regions_set_surface(&r, 0, 3 * one) == SAT_OK);
    p = {3 * one, 0, 3 * one};
    assert(sat_water_regions_find(&r, &p, &index) == SAT_OK && index == 0);
}

void sample_matches_a_single_volume() {
    sat_water_volume_t v[1] = {box(0, 8, 0, 8, -4, 0)};
    sat_water_regions_t r;
    assert(sat_water_regions_init(&r, v, 1) == SAT_OK);
    const sat_vec3_t base = {2 * one, -one, 2 * one};
    sat_water_sample_t want, got;
    uint16_t index;
    assert(sat_water_volume_sample(&v[0], &base, 2 * one, &want) == SAT_OK);
    assert(sat_water_regions_sample(&r, &base, 2 * one, &got, &index) == SAT_OK);
    assert(index == 0 && got.surface == want.surface && got.depth == want.depth &&
           got.fraction == want.fraction && got.inside == want.inside);
    const sat_vec3_t away = {20 * one, -one, 2 * one};
    assert(sat_water_regions_sample(&r, &away, 2 * one, &got, &index) == SAT_OK);
    assert(index == SAT_WATER_REGION_NONE && !got.inside && got.fraction == 0 && got.surface == 0);
    sat_water_regions_t none = {nullptr, 0};
    assert(sat_water_regions_sample(&none, &base, 0, &got, &index) == SAT_OK && !got.inside);
}

void sample_prefers_the_deeper_share() {
    sat_water_volume_t v[2] = {box(0, 8, 0, 8, -4, -3), box(0, 8, 0, 8, -4, 0)};
    sat_water_regions_t r;
    assert(sat_water_regions_init(&r, v, 2) == SAT_OK);
    const sat_vec3_t base = {one, -4 * one, one};
    sat_water_sample_t s;
    uint16_t index;
    assert(sat_water_regions_sample(&r, &base, 4 * one, &s, &index) == SAT_OK);
    assert(index == 1 && s.fraction == SAT_FX16_ONE);
}

void immersion_has_hysteresis_per_region() {
    sat_water_volume_t v[2] = {box(0, 8, 0, 8, -4, 0), box(0, 8, 0, 8, -4, -2)};
    sat_water_regions_t r;
    assert(sat_water_regions_init(&r, v, 2) == SAT_OK);
    const sat_fx16_t h = one / 8;
    uint16_t region;
    sat_vec3_t p = {one, one / 16, one};  // above region 0's surface by less than h
    assert(sat_water_regions_immersion(&r, &p, h, SAT_WATER_REGION_NONE, &region) == SAT_OK);
    assert(region == SAT_WATER_REGION_NONE);
    assert(sat_water_regions_immersion(&r, &p, h, 0, &region) == SAT_OK && region == 0);  // still in
    p.y = -one / 16;
    assert(sat_water_regions_immersion(&r, &p, h, SAT_WATER_REGION_NONE, &region) == SAT_OK);
    assert(region == SAT_WATER_REGION_NONE);  // needs to be below the entry height
    p.y = -one;
    assert(sat_water_regions_immersion(&r, &p, h, SAT_WATER_REGION_NONE, &region) == SAT_OK && region == 0);
    p.y = -3 * one;  // under both: the higher surface wins
    assert(sat_water_regions_immersion(&r, &p, h, SAT_WATER_REGION_NONE, &region) == SAT_OK && region == 0);
    p = {one, -one, 20 * one};
    assert(sat_water_regions_immersion(&r, &p, h, 0, &region) == SAT_OK && region == SAT_WATER_REGION_NONE);
    assert(sat_water_regions_immersion(&r, &p, -1, 0, &region) == SAT_ERR_INVALID_ARG);
}

}

int main() {
    init_validation();
    find_picks_the_highest_surface();
    sample_matches_a_single_volume();
    sample_prefers_the_deeper_share();
    immersion_has_hysteresis_per_region();
}
