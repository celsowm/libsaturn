#include <cassert>
#include <cstdint>
#include <initializer_list>
#include "saturn/water_actor.h"

namespace {

constexpr sat_fx16_t one = SAT_FX16_ONE;
constexpr uint16_t neutral = 0x4210u;

struct Space {
    uint16_t above_i[4 * 16], above_m[16], below_i[4 * 16], below_m[16], wet[32], piece_m[16];
    sat_water_shaded_piece_t pieces[16];
    sat_projected_vertex_t scratch[32];
    sat_water_actor_workspace_t ws{};
    Space() {
        ws.above_indices = above_i; ws.above_materials = above_m;
        ws.below_indices = below_i; ws.below_materials = below_m;
        ws.wet_light = wet; ws.pieces = pieces; ws.piece_materials = piece_m;
        ws.scratch = scratch; ws.face_cap = 16; ws.vertex_cap = 32; ws.piece_cap = 16;
    }
};

sat_water_actor_params_t params() {
    sat_water_actor_params_t p{};
    p.surface = 0; p.tint[0] = -8; p.tint[1] = -2; p.tint[2] = 4; p.full_depth = 2 * one;
    p.pass_below = 1; p.pass_above = 3;
    return p;
}

sat_scene3d_material_t rgb() { sat_scene3d_material_t m{}; m.kind = SAT_SCENE3D_RGB; return m; }
sat_scene3d_material_t textured() { sat_scene3d_material_t m{}; m.kind = SAT_SCENE3D_INDEXED_TEXTURED; return m; }

/* A stack of three quads: wholly above, crossing, wholly below. */
void partitions_a_stack() {
    sat_vec3_t w[12] = {
        {0, 3 * one, 0}, {one, 3 * one, 0}, {one, 1 * one, 0}, {0, 1 * one, 0},     // above
        {0, 1 * one, 0}, {one, 1 * one, 0}, {one, -1 * one, 0}, {0, -1 * one, 0},   // crossing
        {0, -1 * one, 0}, {one, -1 * one, 0}, {one, -3 * one, 0}, {0, -3 * one, 0}, // below
    };
    uint16_t idx[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    sat_mesh_t mesh{w, idx, 12, 12, 3, 3};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Space space;
    sat_water_actor_split_t split;
    const auto p = params();
    assert(sat_water_actor_split(&mesh, nullptr, materials, 1, nullptr, &p, &space.ws, &split) == SAT_OK);
    assert(split.above_faces == 1 && split.below_faces == 1 && split.pieces == 2 && split.uncut_faces == 0);
    assert(space.above_i[0] == 0 && space.below_i[0] == 8);
    unsigned dry = 0, wet = 0;
    for (unsigned i = 0; i < split.pieces; ++i) {
        (space.pieces[i].piece.submerged ? wet : dry)++;
        for (unsigned c = 0; c < 4; ++c) {
            const sat_vec3_t& q = space.pieces[i].piece.quad.v[c];
            assert(space.pieces[i].piece.submerged ? q.y <= 0 : q.y >= 0);
            // Tint is zero on the waterline and grows downward.
            if (q.y == 0) assert(space.pieces[i].color[c] == neutral);
        }
    }
    assert(dry == 1 && wet == 1);
    // The vertex at -1 of 2 full-depth units is half tinted; at -3 fully.
    assert((space.wet[6] & 31) == 16 - 4);
    assert((space.wet[10] & 31) == 16 - 8);
    assert(space.wet[0] == neutral);
}

void textured_faces_follow_their_middle() {
    sat_vec3_t v[8] = {
        {0, -3 * one, 0}, {one, -3 * one, 0}, {one, 1 * one, 0}, {0, 1 * one, 0},   // middle at -1
        {0, -1 * one, 0}, {one, -1 * one, 0}, {one, 3 * one, 0}, {0, 3 * one, 0},   // middle at +1
    };
    uint16_t idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    sat_mesh_t mesh{v, idx, 8, 8, 2, 2};
    const sat_scene3d_material_t materials[1] = {textured()};
    Space space;
    sat_water_actor_split_t split;
    const auto p = params();
    assert(sat_water_actor_split(&mesh, nullptr, materials, 1, nullptr, &p, &space.ws, &split) == SAT_OK);
    assert(split.pieces == 0 && split.uncut_faces == 2 && split.below_faces == 1 && split.above_faces == 1);
    assert(space.below_i[0] == 0 && space.above_i[0] == 4);
}

void materials_are_carried() {
    sat_vec3_t v[8] = {
        {0, 1 * one, 0}, {one, 1 * one, 0}, {one, 2 * one, 0}, {0, 2 * one, 0},
        {0, -2 * one, 0}, {one, -2 * one, 0}, {one, -1 * one, 0}, {0, -1 * one, 0},
    };
    uint16_t idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    const uint16_t face_materials[2] = {1, 0};
    sat_mesh_t mesh{v, idx, 8, 8, 2, 2};
    const sat_scene3d_material_t materials[2] = {rgb(), rgb()};
    Space space;
    sat_water_actor_split_t split;
    const auto p = params();
    assert(sat_water_actor_split(&mesh, face_materials, materials, 2, nullptr, &p, &space.ws, &split) == SAT_OK);
    assert(split.above_faces == 1 && space.above_m[0] == 1);
    assert(split.below_faces == 1 && space.below_m[0] == 0);
    const uint16_t bad[2] = {2, 0};
    assert(sat_water_actor_split(&mesh, bad, materials, 2, nullptr, &p, &space.ws, &split) == SAT_ERR_INVALID_ARG);
}

void follows_a_moving_surface() {
    sat_vec3_t v[4] = {{0, -2 * one, 0}, {one, -2 * one, 0}, {one, 2 * one, 0}, {0, 2 * one, 0}};
    uint16_t idx[4] = {0, 1, 2, 3};
    sat_mesh_t mesh{v, idx, 4, 4, 1, 1};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Space space;
    sat_water_actor_split_t split;
    auto p = params();
    for (sat_fx16_t surface : {-3 * one, 0, 3 * one}) {
        p.surface = surface;
        assert(sat_water_actor_split(&mesh, nullptr, materials, 1, nullptr, &p, &space.ws, &split) == SAT_OK);
        if (surface == 0) assert(split.pieces == 2 && split.above_faces == 0 && split.below_faces == 0);
        else if (surface < 0) assert(split.above_faces == 1 && split.below_faces == 0 && split.pieces == 0);
        else assert(split.below_faces == 1 && split.above_faces == 0 && split.pieces == 0);
    }
}

void capacity_and_validation() {
    sat_vec3_t v[4] = {{0, -2 * one, 0}, {one, -2 * one, 0}, {one, 2 * one, 0}, {0, 2 * one, 0}};
    uint16_t idx[4] = {0, 1, 2, 3};
    sat_mesh_t mesh{v, idx, 4, 4, 1, 1};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Space space;
    sat_water_actor_split_t split;
    auto p = params();
    space.ws.piece_cap = 1;  // a cut makes two pieces
    assert(sat_water_actor_split(&mesh, nullptr, materials, 1, nullptr, &p, &space.ws, &split) == SAT_ERR_CAPACITY);
    space.ws.piece_cap = 16; space.ws.vertex_cap = 2;
    assert(sat_water_actor_split(&mesh, nullptr, materials, 1, nullptr, &p, &space.ws, &split) == SAT_ERR_CAPACITY);
    space.ws.vertex_cap = 32;
    p.full_depth = 0;
    assert(sat_water_actor_split(&mesh, nullptr, materials, 1, nullptr, &p, &space.ws, &split) == SAT_ERR_INVALID_ARG);
    p = params();
    assert(sat_water_actor_split(&mesh, nullptr, materials, 17, nullptr, &p, &space.ws, &split) == SAT_ERR_INVALID_ARG);
    assert(sat_water_actor_split(nullptr, nullptr, materials, 1, nullptr, &p, &space.ws, &split) == SAT_ERR_INVALID_ARG);
}

}

int main() {
    partitions_a_stack();
    textured_faces_follow_their_middle();
    materials_are_carried();
    follows_a_moving_surface();
    capacity_and_validation();
}
