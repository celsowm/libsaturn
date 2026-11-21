#include <cassert>
#include <cstdint>
#include "saturn/water_prepare.h"

namespace {

constexpr sat_fx16_t one = SAT_FX16_ONE;
constexpr uint16_t neutral = 0x4210u;

int red(uint16_t c) { return c & 31; }
int green(uint16_t c) { return (c >> 5) & 31; }
int blue(uint16_t c) { return (c >> 10) & 31; }

struct Level {
    sat_water_volume_t volumes[2];
    sat_water_regions_t regions;
    Level() {
        volumes[0] = {};
        volumes[0].min_x = -8 * one; volumes[0].max_x = 8 * one;
        volumes[0].min_z = -8 * one; volumes[0].max_z = 8 * one;
        volumes[0].bottom = -8 * one; volumes[0].surface = 0;
        volumes[0].drag = one / 5;
        volumes[1] = volumes[0];
        volumes[1].min_x = 20 * one; volumes[1].max_x = 28 * one;
        volumes[1].surface = 2 * one;
        assert(sat_water_regions_init(&regions, volumes, 2) == SAT_OK);
    }
};

struct Output {
    sat_vec3_t vertices[64], scratch_vertices[64];
    uint16_t gouraud[64], scratch_gouraud[64], scratch_remap[64], indices[4 * 32], face_materials[32];
    sat_water_prepared_t io{};
    Output() {
        io.vertices = vertices; io.gouraud = gouraud; io.indices = indices;
        io.face_materials = face_materials; io.vertex_cap = 64; io.face_cap = 32;
        io.scratch_vertices = scratch_vertices; io.scratch_gouraud = scratch_gouraud;
        io.scratch_remap = scratch_remap;
    }
    /* Corner c of output face f: above faces index the first block, below faces
     * the block at below_first. */
    const sat_vec3_t& corner(unsigned f, unsigned c) const {
        return vertices[(f < io.above_faces ? 0u : io.below_first) + indices[4 * f + c]];
    }
    uint16_t light(unsigned f, unsigned c) const {
        return gouraud[(f < io.above_faces ? 0u : io.below_first) + indices[4 * f + c]];
    }
};

sat_water_prepare_config_t config(const Level& level) {
    sat_water_prepare_config_t c{};
    c.regions = &level.regions;
    c.tint[0] = -8; c.tint[1] = -3; c.tint[2] = 2;
    c.full_depth = 4 * one;
    return c;
}

sat_scene3d_material_t rgb() { sat_scene3d_material_t m{}; m.kind = SAT_SCENE3D_RGB; return m; }
sat_scene3d_material_t textured() {
    sat_scene3d_material_t m{}; m.kind = SAT_SCENE3D_INDEXED_TEXTURED; return m;
}

/* Quads: 0 floor (below), 1 ledge (above), 2 wall crossing the surface. */
void classify_and_cut() {
    Level level;
    sat_vec3_t v[12] = {
        {-4 * one, -6 * one, -4 * one}, {4 * one, -6 * one, -4 * one},
        {4 * one, -6 * one, 4 * one}, {-4 * one, -6 * one, 4 * one},
        {-4 * one, 2 * one, -4 * one}, {4 * one, 2 * one, -4 * one},
        {4 * one, 2 * one, 4 * one}, {-4 * one, 2 * one, 4 * one},
        {-4 * one, -4 * one, 6 * one}, {4 * one, -4 * one, 6 * one},
        {4 * one, 4 * one, 6 * one}, {-4 * one, 4 * one, 6 * one},
    };
    uint16_t idx[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    sat_mesh_t mesh{v, idx, 12, 12, 3, 3};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Output out;
    const auto c = config(level);
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &c, &out.io) == SAT_OK);
    assert(out.io.cut_faces == 1 && out.io.uncut_faces == 0);
    // Wall cut into one above and one below piece: 1 above (ledge) + 1, 1 below + 1.
    assert(out.io.above_faces == 2 && out.io.below_faces == 2);
    // Each side has its own block: the ledge and the dry piece above, the floor
    // and the wet piece below, four vertices each.
    assert(out.io.above_vertices == 8 && out.io.below_first == 8 &&
           out.io.below_vertices == 8 && out.io.vertex_count == 16);
    // Above faces first: every one lies at or over the surface (y >= 0).
    for (unsigned f = 0; f < out.io.above_faces; ++f)
        for (unsigned k = 0; k < 4; ++k) assert(out.corner(f, k).y >= 0);
    for (unsigned f = out.io.above_faces; f < out.io.above_faces + out.io.below_faces; ++f)
        for (unsigned k = 0; k < 4; ++k) assert(out.corner(f, k).y <= 0);
    // The cut is exact on both sides: both pieces carry the same two edge points.
    sat_vec3_t above_edge[2], below_edge[2];
    unsigned on_surface_above = 0, on_surface_below = 0;
    for (unsigned f = 0; f < out.io.above_faces + out.io.below_faces; ++f)
        for (unsigned k = 0; k < 4; ++k)
            if (out.corner(f, k).y == 0) {
                if (f < out.io.above_faces) above_edge[on_surface_above++ % 2] = out.corner(f, k);
                else below_edge[on_surface_below++ % 2] = out.corner(f, k);
            }
    assert(on_surface_above == 2 && on_surface_below == 2);
    for (unsigned a = 0; a < 2; ++a) {
        bool found = false;
        for (unsigned b = 0; b < 2; ++b)
            found |= above_edge[a].x == below_edge[b].x && above_edge[a].z == below_edge[b].z;
        assert(found);
    }
    // No index reaches past its own block.
    for (unsigned f = 0; f < out.io.above_faces; ++f)
        for (unsigned k = 0; k < 4; ++k) assert(out.indices[4 * f + k] < out.io.above_vertices);
    for (unsigned f = out.io.above_faces; f < out.io.above_faces + out.io.below_faces; ++f)
        for (unsigned k = 0; k < 4; ++k) assert(out.indices[4 * f + k] < out.io.below_vertices);
    for (unsigned f = 0; f < 4; ++f) assert(out.face_materials[f] == 0);
}

void tint_grows_with_depth() {
    Level level;
    sat_vec3_t v[8] = {
        {0, 0, 0}, {one, 0, 0}, {one, -2 * one, 0}, {0, -2 * one, 0},   // down to 2 units
        {2 * one, -8 * one, 0}, {3 * one, -8 * one, 0}, {3 * one, -6 * one, 0}, {2 * one, -6 * one, 0},
    };
    uint16_t idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    sat_mesh_t mesh{v, idx, 8, 8, 2, 2};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Output out;
    const auto c = config(level);
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &c, &out.io) == SAT_OK);
    // A face touching the surface from below is under it.
    assert(out.io.vertex_count == 8 && out.io.above_faces == 0 && out.io.below_faces == 2);
    // The below faces come out in reverse order: the deep quad (input vertices
    // 4..7) first, then the shallow one (0..3).
    const auto at = [&](unsigned input) { return out.gouraud[input < 4 ? input + 4 : input - 4]; };
    assert(at(0) == neutral && at(1) == neutral);  // on the surface: no tint
    // 2 of 4 units deep: half the tint (red -4, green -1.5 -> -1, blue +1).
    assert(red(at(2)) == 16 - 4 && green(at(2)) == 16 - 1 && blue(at(2)) == 16 + 1);
    // Past full depth: the whole tint, and no more.
    assert(red(at(4)) == 16 - 8 && green(at(4)) == 16 - 3 && blue(at(4)) == 16 + 2);
    assert(red(at(6)) == 16 - 8);
}

void base_light_is_kept_and_clamped() {
    Level level;
    sat_vec3_t v[4] = {{0, -6 * one, 0}, {one, -6 * one, 0}, {one, -6 * one, one}, {0, -6 * one, one}};
    uint16_t idx[4] = {0, 1, 2, 3};
    sat_mesh_t mesh{v, idx, 4, 4, 1, 1};
    const uint16_t base[4] = {sat_gouraud_rgb(4, 4, 4), sat_gouraud_rgb(-14, 0, 0), neutral, neutral};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Output out;
    const auto c = config(level);
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, base, &c, &out.io) == SAT_OK);
    assert(red(out.gouraud[0]) == 16 + 4 - 8 && blue(out.gouraud[0]) == 16 + 4 + 2);
    assert(red(out.gouraud[1]) == 0);  // clamped at zero, not wrapped
}

void textured_crossing_is_not_cut() {
    Level level;
    sat_vec3_t v[8] = {
        {-4 * one, -4 * one, 0}, {4 * one, -4 * one, 0}, {4 * one, 4 * one, 0}, {-4 * one, 4 * one, 0},
        {-4 * one, -3 * one, 2 * one}, {4 * one, -3 * one, 2 * one}, {4 * one, 5 * one, 2 * one}, {-4 * one, 5 * one, 2 * one},
    };
    uint16_t idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    const uint16_t face_materials[2] = {0, 0};
    sat_mesh_t mesh{v, idx, 8, 8, 2, 2};
    const sat_scene3d_material_t materials[1] = {textured()};
    Output out;
    const auto c = config(level);
    assert(sat_water_prepare_mesh(&mesh, face_materials, materials, 1, nullptr, &c, &out.io) == SAT_OK);
    assert(out.io.cut_faces == 0 && out.io.uncut_faces == 2 && out.io.vertex_count == 8);
    assert(out.io.above_vertices == 8 && out.io.below_vertices == 0);
    // Face 0 straddles evenly (centroid at 0, not under): above. Face 1's
    // middle is at y = 1, above the water too.
    assert(out.io.above_faces == 2 && out.io.below_faces == 0);
    // Lowered into the water the centroid goes under, and it joins below.
    v[0].y = v[1].y = -8 * one; v[2].y = v[3].y = 4 * one - one;
    assert(sat_water_prepare_mesh(&mesh, face_materials, materials, 1, nullptr, &c, &out.io) == SAT_OK);
    assert(out.io.uncut_faces == 2 && out.io.below_faces >= 1);
}

void regions_choose_their_own_surface() {
    Level level;
    sat_vec3_t v[8] = {
        {-4 * one, -1 * one, 0}, {4 * one, -1 * one, 0}, {4 * one, 1 * one, 0}, {-4 * one, 1 * one, 0},
        {22 * one, -1 * one, 0}, {26 * one, -1 * one, 0}, {26 * one, 1 * one, 0}, {22 * one, 1 * one, 0},
    };
    uint16_t idx[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    sat_mesh_t mesh{v, idx, 8, 8, 2, 2};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Output out;
    const auto c = config(level);
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &c, &out.io) == SAT_OK);
    // The first wall crosses y = 0; the second lies wholly under its surface at y = 2.
    assert(out.io.cut_faces == 1 && out.io.below_faces == 2 && out.io.above_faces == 1);
}

/* Two walls stacked on the surface share their edge vertices; each side keeps
 * its own copy so an instance projects only what it draws, and vertices a side
 * shares between its own faces are stored once. */
void waterline_vertices_live_in_both_blocks() {
    Level level;
    sat_vec3_t v[6] = {
        {0, 2 * one, 0}, {one, 2 * one, 0}, {one, 0, 0}, {0, 0, 0},      // above
        {one, -2 * one, 0}, {0, -2 * one, 0},                              // below, sharing 2 and 3
    };
    uint16_t idx[8] = {0, 1, 2, 3, 3, 2, 4, 5};
    sat_mesh_t mesh{v, idx, 6, 6, 2, 2};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Output out;
    const auto c = config(level);
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &c, &out.io) == SAT_OK);
    assert(out.io.above_faces == 1 && out.io.below_faces == 1);
    assert(out.io.above_vertices == 4 && out.io.below_vertices == 4 && out.io.vertex_count == 8);
    assert(out.corner(0, 2).y == 0 && out.corner(1, 0).y == 0);
    // Each block holds the waterline points exactly once.
    unsigned waterline = 0;
    for (unsigned k = 0; k < out.io.vertex_count; ++k) waterline += out.vertices[k].y == 0;
    assert(waterline == 4);
}

void no_region_changes_nothing() {
    sat_water_regions_t none = {nullptr, 0};
    sat_vec3_t v[4] = {{0, -6 * one, 0}, {one, -6 * one, 0}, {one, 2 * one, 0}, {0, 2 * one, 0}};
    uint16_t idx[4] = {0, 1, 2, 3};
    sat_mesh_t mesh{v, idx, 4, 4, 1, 1};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Output out;
    sat_water_prepare_config_t c{};
    c.regions = &none; c.tint[0] = -8; c.full_depth = one;
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &c, &out.io) == SAT_OK);
    assert(out.io.above_faces == 1 && out.io.below_faces == 0 && out.io.cut_faces == 0);
    for (unsigned k = 0; k < 4; ++k) assert(out.vertices[k].y == v[k].y && out.gouraud[k] == neutral);
}

void capacity_and_validation() {
    Level level;
    sat_vec3_t v[4] = {{-4 * one, -4 * one, 0}, {4 * one, -4 * one, 0}, {4 * one, 4 * one, 0}, {-4 * one, 4 * one, 0}};
    uint16_t idx[4] = {0, 1, 2, 3};
    sat_mesh_t mesh{v, idx, 4, 4, 1, 1};
    const sat_scene3d_material_t materials[1] = {rgb()};
    Output out;
    const auto c = config(level);
    out.io.face_cap = 1;  // the cut needs two faces
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &c, &out.io) == SAT_ERR_CAPACITY);
    out.io.face_cap = 32; out.io.vertex_cap = 5;  // the cut needs eight new vertices
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &c, &out.io) == SAT_ERR_CAPACITY);
    out.io.vertex_cap = 64;
    const uint16_t bad[1] = {3};
    assert(sat_water_prepare_mesh(&mesh, bad, materials, 1, nullptr, &c, &out.io) == SAT_ERR_INVALID_ARG);
    auto zero = c; zero.full_depth = 0;
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &zero, &out.io) == SAT_ERR_INVALID_ARG);
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &c, nullptr) == SAT_ERR_INVALID_ARG);
    assert(sat_water_prepare_mesh(&mesh, nullptr, materials, 1, nullptr, &c, &out.io) == SAT_OK);
}

}

int main() {
    classify_and_cut();
    tint_grows_with_depth();
    base_light_is_kept_and_clamped();
    textured_crossing_is_not_cut();
    regions_choose_their_own_surface();
    waterline_vertices_live_in_both_blocks();
    no_region_changes_nothing();
    capacity_and_validation();
}
