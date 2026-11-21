#include <cassert>
#include <initializer_list>
#include "saturn/water.h"
#include "saturn/water_volume.h"
#include "src/graphics/3d/rendering/logic.hpp"

static void split() {
    const sat_quad3_t face = {{{INT32_MIN,INT32_MIN,0}, {INT32_MAX,INT32_MAX,0},
                              {INT32_MAX,INT32_MAX,40}, {INT32_MIN,INT32_MIN,40}}};
    const sat_quad3_t reversed = {{face.v[1], face.v[0], face.v[3], face.v[2]}};
    sat_water_piece_t pieces[4], back[4];
    uint8_t count = 0, reverse_count = 0;
    assert(sat_water_split_quad(&face, -2, pieces, &count) == SAT_OK && count == 2);
    assert(sat_water_split_quad(&reversed, -2, back, &reverse_count) == SAT_OK && reverse_count == count);
    for (unsigned i = 0; i < count; ++i) {
        unsigned shared = 0;
        for (const auto& v : pieces[i].quad.v) {
            assert(pieces[i].submerged ? v.y <= -2 : v.y >= -2);
            if (v.y != -2) continue;
            assert(v.x == -2);
            for (const auto& r : back[i].quad.v) {
                if (v.x == r.x && v.y == r.y && v.z == r.z) ++shared;
            }
        }
        assert(shared == 2);
    }

    const sat_quad3_t point_on_line = {{{0,0,0}, {8,8,0}, {8,0,8}, {0,-8,8}}};
    assert(sat_water_split_quad(&point_on_line, 0, pieces, &count) == SAT_OK && count == 2);
    assert(pieces[0].quad.v[2].y == 0 && pieces[1].quad.v[1].y == 0);
    assert(!pieces[0].submerged && pieces[1].submerged);
    const sat_quad3_t flat = {{{0,1,0}, {8,1,0}, {8,1,8}, {0,1,8}}};
    assert(sat_water_split_quad(&flat, 1, pieces, &count) == SAT_OK && count == 1);
    assert(!pieces[0].submerged);
    assert(sat_water_split_quad(nullptr, 1, pieces, &count) == SAT_ERR_INVALID_ARG && count == 1);
}

static void shaded_split() {
    constexpr uint16_t black = 0, white = 0x7FFF;
    const uint16_t colors[4] = {black, white, white, black};
    const sat_quad3_t face = {{{0,-8,0}, {0,8,0}, {4,8,0}, {4,-8,0}}};
    const sat_quad3_t reversed = {{face.v[1], face.v[0], face.v[3], face.v[2]}};
    const uint16_t reversed_colors[4] = {colors[1], colors[0], colors[3], colors[2]};
    sat_water_shaded_piece_t pieces[4], back[4];
    uint8_t count = 0, reverse_count = 0;
    assert(sat_water_split_quad_shaded(&face, colors, 0, pieces, &count) == SAT_OK && count == 2);
    assert(sat_water_split_quad_shaded(&reversed, reversed_colors, 0, back, &reverse_count) == SAT_OK &&
           reverse_count == count);
    for (unsigned i = 0; i < count; ++i) {
        unsigned cut = 0;
        for (unsigned v = 0; v < 4; ++v) {
            if (pieces[i].piece.quad.v[v].y != 0) continue;
            assert(pieces[i].color[v] == 0x3DEFu);  // mid-grey: 15 of 31 per channel
            ++cut;
        }
        assert(cut == 2);
    }
    for (unsigned i = 0; i < reverse_count; ++i) {
        for (unsigned v = 0; v < 4; ++v) {
            if (back[i].piece.quad.v[v].y == 0) assert(back[i].color[v] == 0x3DEFu);
        }
    }
    // A face wholly on one side keeps its corner colors untouched.
    assert(sat_water_split_quad_shaded(&face, colors, 100, pieces, &count) == SAT_OK && count == 1);
    for (unsigned v = 0; v < 4; ++v) assert(pieces[0].color[v] == colors[v]);
    assert(sat_water_split_quad_shaded(&face, nullptr, 0, pieces, &count) == SAT_ERR_INVALID_ARG);
}

static void water_and_near_plane() {
    constexpr sat_fx16_t one = SAT_FX16_ONE;
    const sat_quad3_t face = {{{0,-one,0}, {0,one,0}, {0,one,3*one}, {0,-one,3*one}}};
    const sat_vec3_t eye = {0,0,0};
    const sat_vec3_t forward = {0,0,one};
    sat_water_piece_t pieces[4];
    uint8_t count;
    assert(sat_water_split_quad(&face, 0, pieces, &count) == SAT_OK && count == 2);
    for (unsigned i = 0; i < count; ++i) {
        sat_quad3_t clipped[4];
        uint8_t clipped_count;
        assert(saturn::core::render3d::clip_world_quad_near(
            &pieces[i].quad,&eye,&forward,one,clipped,&clipped_count) == SAT_OK);
        bool shared_corner = false;
        for (unsigned n = 0; n < clipped_count; ++n) {
            for (const auto& v : clipped[n].v) {
                assert(v.z >= one);
                assert(pieces[i].submerged ? v.y <= 0 : v.y >= 0);
                shared_corner |= v.y == 0 && v.z == one;
            }
        }
        assert(shared_corner);
    }
}

static void volume() {
    constexpr sat_fx16_t one = SAT_FX16_ONE;
    sat_water_volume_t v = {-one,one,-one,one,-2*one,0,one,0,one/4,one/2};
    sat_vec3_t p = {0,-one/2,0};
    sat_water_sample_t sample{};
    assert(sat_water_volume_sample(&v,&p,one,&sample) == SAT_OK);
    assert(sample.inside && sample.depth == one/2 && sample.fraction == one/2);
    sat_vec3_t velocity{};
    assert(sat_water_volume_apply(&v,&sample,&velocity) == SAT_OK);
    assert(velocity.x == one/4 && velocity.y == one/8 && velocity.z == 0);

    uint8_t immersed = 0;
    for (const sat_fx16_t y : {-one/32,-one/4,one/32,one/4}) {
        p.y = y;
        assert(sat_water_volume_immersion(&v,&p,one/16,immersed,&immersed) == SAT_OK);
        assert(immersed == (y == -one/4 || y == one/32));
    }
    p.x = 2*one;
    assert(sat_water_volume_sample(&v,&p,one,&sample) == SAT_OK);
    assert(!sample.inside && sample.fraction == 0);
    const sat_vec3_t dry = {INT32_MIN,INT32_MAX,one};
    velocity = dry;
    for (int i = 0; i < 60; ++i) {
        assert(sat_water_volume_apply(&v,&sample,&velocity) == SAT_OK);
    }
    assert(velocity.x == dry.x && velocity.y == dry.y && velocity.z == dry.z);

    v.bottom = INT32_MIN;
    v.surface = INT32_MAX;
    v.buoyancy = INT32_MAX;
    v.drag = 0;
    p = {0,INT32_MIN,0};
    assert(sat_water_volume_sample(&v,&p,INT32_MAX,&sample) == SAT_OK);
    assert(sample.depth == INT32_MAX && sample.fraction == one);
    assert(sat_water_volume_apply(&v,&sample,&velocity) == SAT_OK && velocity.y == INT32_MAX);
}

int main() {
    split();
    shaded_split();
    water_and_near_plane();
    volume();
}
