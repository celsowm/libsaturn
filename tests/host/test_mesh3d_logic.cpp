/* test_mesh3d_logic.cpp — host tests for quad meshes and primitive solids.
 *
 * The property that actually matters on this hardware is the winding: every
 * face of a solid must have its outward normal pointing away from the solid,
 * or backface culling removes exactly the faces it should have kept. Checking
 * that directly, for every face of every primitive, is worth more than
 * checking individual corner coordinates -- a builder can have all the right
 * vertices in the wrong order and still pass a coordinate test.
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/math3d.h"
#include "saturn/mesh3d.h"
#include "src/core/mesh3d_logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)
#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_NEAR(a, b, tol) do { \
    long long _d = (long long)(a) - (long long)(b); \
    if (_d < 0) _d = -_d; \
    if (_d > (tol)) { \
        fprintf(stderr, "FAIL %s:%d: %s (%ld) not within %ld of %s (%ld)\n", \
                __FILE__, __LINE__, #a, (long)(a), (long)(tol), #b, (long)(b)); \
        exit(1); } } while(0)

using namespace saturn::core::mesh3d;
using saturn::core::math3d::fx_abs;
using saturn::core::math3d::fx_from_int;
using saturn::core::math3d::fx_to_int;
using saturn::core::math3d::vec3;
using saturn::core::math3d::vec3_dot_raw;
using saturn::core::math3d::vec3_sub;

namespace {

constexpr uint16_t kVertexCap = 512;
constexpr uint16_t kFaceCap = 512;

sat_vec3_t g_vertices[kVertexCap];
uint16_t g_indices[kFaceCap * 4];

sat_mesh_t make_mesh() {
    sat_mesh_t mesh;
    ASSERT_EQ(init(&mesh, g_vertices, kVertexCap, g_indices, kFaceCap), SAT_OK);
    return mesh;
}

/* Every face normal must point away from the solid's centre. */
void assert_outward(const sat_mesh_t& mesh, const sat_vec3_t& center, const char* what) {
    for (uint16_t i = 0; i < mesh.face_count; ++i) {
        sat_vec3_t normal;
        sat_vec3_t fc;
        ASSERT_EQ(face_normal(&mesh, i, &normal), SAT_OK);
        ASSERT_EQ(face_center(&mesh, i, &fc), SAT_OK);
        if (vec3_dot_raw(normal, vec3_sub(fc, center)) <= 0) {
            fprintf(stderr, "FAIL %s: face %u of %s winds inward\n", __func__, i, what);
            exit(1);
        }
    }
}

}  // namespace

TEST(init_rejects_null_storage) {
    sat_mesh_t mesh;
    ASSERT_EQ(init(&mesh, nullptr, 8, g_indices, 6), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(init(&mesh, g_vertices, 8, nullptr, 6), SAT_ERR_INVALID_ARG);
}

TEST(add_face_rejects_unknown_vertices) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(add_vertex(&mesh, 0, 0, 0, nullptr), SAT_OK);
    ASSERT_EQ(add_face(&mesh, 0, 0, 0, 1), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(mesh.face_count, 0u);
}

TEST(add_vertex_reports_capacity) {
    sat_mesh_t mesh;
    sat_vec3_t storage[2];
    uint16_t indices[4];
    ASSERT_EQ(init(&mesh, storage, 2, indices, 1), SAT_OK);
    ASSERT_EQ(add_vertex(&mesh, 0, 0, 0, nullptr), SAT_OK);
    ASSERT_EQ(add_vertex(&mesh, 0, 0, 0, nullptr), SAT_OK);
    ASSERT_EQ(add_vertex(&mesh, 0, 0, 0, nullptr), SAT_ERR_CAPACITY);
}

TEST(add_quad_appends_four_vertices_and_one_face) {
    sat_mesh_t mesh = make_mesh();
    sat_quad3_t quad;
    quad.v[0] = vec3(fx_from_int(-1), fx_from_int(1), 0);
    quad.v[1] = vec3(fx_from_int(1), fx_from_int(1), 0);
    quad.v[2] = vec3(fx_from_int(1), fx_from_int(-1), 0);
    quad.v[3] = vec3(fx_from_int(-1), fx_from_int(-1), 0);
    ASSERT_EQ(add_quad(&mesh, &quad), SAT_OK);
    ASSERT_EQ(mesh.vertex_count, 4u);
    ASSERT_EQ(mesh.face_count, 1u);

    /* A, B, C, D as a VDP1 quad: the outward normal faces the viewer at +Z. */
    sat_vec3_t normal;
    ASSERT_EQ(face_normal(&mesh, 0, &normal), SAT_OK);
    ASSERT_EQ(normal.x, 0);
    ASSERT_EQ(normal.y, 0);
    ASSERT_TRUE(normal.z > 0);
}

TEST(counts_match_what_the_builders_produce) {
    sat_mesh_t mesh = make_mesh();
    uint16_t v = 0;
    uint16_t f = 0;

    box_counts(&v, &f);
    ASSERT_EQ(build_box(&mesh, vec3(0, 0, 0), 1, 1, 1), SAT_OK);
    ASSERT_EQ(mesh.vertex_count, v);
    ASSERT_EQ(mesh.face_count, f);

    plane_counts(4, 3, &v, &f);
    ASSERT_EQ(build_plane(&mesh, vec3(0, 0, 0), fx_from_int(8), fx_from_int(6), 4, 3), SAT_OK);
    ASSERT_EQ(mesh.vertex_count, v);
    ASSERT_EQ(mesh.face_count, f);

    sphere_counts(8, 5, &v, &f);
    ASSERT_EQ(build_sphere(&mesh, vec3(0, 0, 0), fx_from_int(10), 8, 5), SAT_OK);
    ASSERT_EQ(mesh.vertex_count, v);
    ASSERT_EQ(mesh.face_count, f);

    cylinder_counts(7, true, &v, &f);
    ASSERT_EQ(build_cylinder(&mesh, vec3(0, 0, 0), fx_from_int(4), fx_from_int(6), 7, true), SAT_OK);
    ASSERT_EQ(mesh.vertex_count, v);
    ASSERT_EQ(mesh.face_count, f);

    cylinder_counts(7, false, &v, &f);
    ASSERT_EQ(build_cylinder(&mesh, vec3(0, 0, 0), fx_from_int(4), fx_from_int(6), 7, false), SAT_OK);
    ASSERT_EQ(mesh.vertex_count, v);
    ASSERT_EQ(mesh.face_count, f);
}

TEST(box_faces_wind_outward) {
    sat_mesh_t mesh = make_mesh();
    const sat_vec3_t center = vec3(fx_from_int(20), fx_from_int(5), fx_from_int(-30));
    ASSERT_EQ(
        build_box(&mesh, center, fx_from_int(4), fx_from_int(12), fx_from_int(7)), SAT_OK);
    assert_outward(mesh, center, "box");
}

/* A wall run merged out of a maze is long and thin: the cross product of a
 * 200-unit edge with a 12-unit one overflows a naive 16.16 cross, which is
 * exactly the case that used to cull a whole wall. */
TEST(long_thin_box_still_winds_outward) {
    sat_mesh_t mesh = make_mesh();
    const sat_vec3_t center = vec3(fx_from_int(112), fx_from_int(6), fx_from_int(100));
    ASSERT_EQ(
        build_box(&mesh, center, fx_from_int(112), fx_from_int(6), fx_from_int(4)), SAT_OK);
    assert_outward(mesh, center, "long box");
}

TEST(plane_normal_points_up) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(
        build_plane(&mesh, vec3(0, fx_from_int(3), 0), fx_from_int(100), fx_from_int(80), 4, 4),
        SAT_OK);
    ASSERT_EQ(mesh.face_count, 16u);
    for (uint16_t i = 0; i < mesh.face_count; ++i) {
        sat_vec3_t n;
        ASSERT_EQ(face_normal(&mesh, i, &n), SAT_OK);
        ASSERT_EQ(n.x, 0);
        ASSERT_EQ(n.z, 0);
        ASSERT_NEAR(n.y, SAT_FX16_ONE, 64);
    }
}

TEST(plane_spans_the_requested_extent) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(
        build_plane(&mesh, vec3(fx_from_int(50), 0, fx_from_int(20)), fx_from_int(30), fx_from_int(10), 3, 2),
        SAT_OK);
    sat_fx16_t min_x = mesh.vertices[0].x;
    sat_fx16_t max_x = mesh.vertices[0].x;
    sat_fx16_t min_z = mesh.vertices[0].z;
    sat_fx16_t max_z = mesh.vertices[0].z;
    for (uint16_t i = 1; i < mesh.vertex_count; ++i) {
        if (mesh.vertices[i].x < min_x) { min_x = mesh.vertices[i].x; }
        if (mesh.vertices[i].x > max_x) { max_x = mesh.vertices[i].x; }
        if (mesh.vertices[i].z < min_z) { min_z = mesh.vertices[i].z; }
        if (mesh.vertices[i].z > max_z) { max_z = mesh.vertices[i].z; }
    }
    ASSERT_EQ(fx_to_int(min_x), 20);
    ASSERT_EQ(fx_to_int(max_x), 80);
    ASSERT_EQ(fx_to_int(min_z), 10);
    ASSERT_EQ(fx_to_int(max_z), 30);
}

TEST(sphere_faces_wind_outward) {
    sat_mesh_t mesh = make_mesh();
    const sat_vec3_t center = vec3(fx_from_int(-8), fx_from_int(14), fx_from_int(3));
    ASSERT_EQ(build_sphere(&mesh, center, fx_from_int(16), 10, 6), SAT_OK);
    assert_outward(mesh, center, "sphere");
}

TEST(sphere_vertices_sit_on_the_radius) {
    sat_mesh_t mesh = make_mesh();
    const sat_vec3_t center = vec3(fx_from_int(4), fx_from_int(9), fx_from_int(-2));
    ASSERT_EQ(build_sphere(&mesh, center, fx_from_int(20), 8, 5), SAT_OK);
    for (uint16_t i = 0; i < mesh.vertex_count; ++i) {
        const sat_vec3_t d = vec3_sub(mesh.vertices[i], center);
        ASSERT_NEAR(fx_to_int(saturn::core::math3d::vec3_length(d)), 20, 1);
    }
}

TEST(sphere_clamps_degenerate_subdivision) {
    sat_mesh_t mesh = make_mesh();
    /* 1 segment and 0 rings cannot describe a surface; clamping beats
     * returning an empty mesh that draws as nothing with no error. */
    ASSERT_EQ(build_sphere(&mesh, vec3(0, 0, 0), fx_from_int(5), 1, 0), SAT_OK);
    ASSERT_EQ(mesh.face_count, 6u); /* 3 segments x 2 rings */
}

TEST(cylinder_faces_wind_outward) {
    sat_mesh_t mesh = make_mesh();
    const sat_vec3_t center = vec3(fx_from_int(30), fx_from_int(6), fx_from_int(30));
    ASSERT_EQ(
        build_cylinder(&mesh, center, fx_from_int(5), fx_from_int(6), 8, true), SAT_OK);
    assert_outward(mesh, center, "capped cylinder");
}

TEST(uncapped_cylinder_has_only_side_faces) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(
        build_cylinder(&mesh, vec3(0, 0, 0), fx_from_int(5), fx_from_int(6), 8, false), SAT_OK);
    ASSERT_EQ(mesh.face_count, 8u);
    for (uint16_t i = 0; i < mesh.face_count; ++i) {
        sat_vec3_t n;
        ASSERT_EQ(face_normal(&mesh, i, &n), SAT_OK);
        ASSERT_NEAR(n.y, 0, 64); /* side walls are vertical */
    }
}

TEST(builders_report_capacity_without_corrupting_the_mesh) {
    sat_mesh_t mesh;
    sat_vec3_t storage[8];
    uint16_t indices[6 * 4];
    ASSERT_EQ(init(&mesh, storage, 8, indices, 6), SAT_OK);
    ASSERT_EQ(build_box(&mesh, vec3(0, 0, 0), 1, 1, 1), SAT_OK);
    ASSERT_EQ(mesh.face_count, 6u);
    /* A sphere needs far more room; the box must survive untouched. */
    ASSERT_EQ(build_sphere(&mesh, vec3(0, 0, 0), fx_from_int(5), 16, 8), SAT_ERR_CAPACITY);
    ASSERT_EQ(mesh.face_count, 6u);
    ASSERT_EQ(mesh.vertex_count, 8u);
}

TEST(face_visible_matches_which_side_the_eye_is_on) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(
        build_box(&mesh, vec3(0, 0, 0), fx_from_int(10), fx_from_int(10), fx_from_int(10)),
        SAT_OK);

    const sat_vec3_t eye = vec3(fx_from_int(0), fx_from_int(0), fx_from_int(100));
    uint16_t visible = 0;
    for (uint16_t i = 0; i < mesh.face_count; ++i) {
        if (face_visible(&mesh, i, eye)) {
            ++visible;
        }
    }
    /* Looking straight down +Z at an axis-aligned cube: exactly one face. */
    ASSERT_EQ(visible, 1u);

    const sat_vec3_t corner =
        vec3(fx_from_int(100), fx_from_int(100), fx_from_int(100));
    visible = 0;
    for (uint16_t i = 0; i < mesh.face_count; ++i) {
        if (face_visible(&mesh, i, corner)) {
            ++visible;
        }
    }
    ASSERT_EQ(visible, 3u);
}

TEST(translate_moves_every_vertex) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(build_box(&mesh, vec3(0, 0, 0), fx_from_int(2), fx_from_int(2), fx_from_int(2)), SAT_OK);
    ASSERT_EQ(translate(&mesh, fx_from_int(10), fx_from_int(-4), fx_from_int(7)), SAT_OK);
    sat_vec3_t center;
    ASSERT_EQ(face_center(&mesh, 4, &center), SAT_OK); /* +Y face */
    ASSERT_EQ(fx_to_int(center.x), 10);
    ASSERT_EQ(fx_to_int(center.y), -2);
    ASSERT_EQ(fx_to_int(center.z), 7);
}

TEST(clear_keeps_the_storage_binding) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(build_box(&mesh, vec3(0, 0, 0), 1, 1, 1), SAT_OK);
    clear(&mesh);
    ASSERT_EQ(mesh.face_count, 0u);
    ASSERT_EQ(mesh.vertex_count, 0u);
    ASSERT_TRUE(mesh.vertices == g_vertices);
    ASSERT_EQ(build_box(&mesh, vec3(0, 0, 0), 1, 1, 1), SAT_OK);
    ASSERT_EQ(mesh.face_count, 6u);
}


/* face_normal_scaled is the hot path; face_normal is it plus a normalise.
 * They must point the same way for every face of a primitive, or culling and
 * shading would disagree about which side a face is on. */
TEST(scaled_and_unit_normals_point_the_same_way) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(build_sphere(&mesh, vec3(0, 0, 0), fx_from_int(12), 8, 5), SAT_OK);
    for (uint16_t i = 0; i < mesh.face_count; ++i) {
        sat_vec3_t scaled;
        sat_vec3_t unit;
        ASSERT_EQ(face_normal_scaled(&mesh, i, &scaled), SAT_OK);
        ASSERT_EQ(face_normal(&mesh, i, &unit), SAT_OK);
        ASSERT_TRUE(vec3_dot_raw(scaled, unit) > 0);
    }
}


TEST(sphere_wedge_counts_match_the_builder) {
    sat_mesh_t mesh = make_mesh();
    uint16_t v = 0;
    uint16_t f = 0;

    sphere_wedge_counts(12, 3, 4, &v, &f);
    ASSERT_EQ(build_sphere_wedge(&mesh, vec3(0, 0, 0), fx_from_int(10), 12, 3, 2, 4), SAT_OK);
    ASSERT_EQ(mesh.vertex_count, v);
    ASSERT_EQ(mesh.face_count, f);
    /* 3 rings x 8 surviving bands, plus two cut walls of 3 quads each. */
    ASSERT_EQ(mesh.face_count, 30u);

    sphere_wedge_counts(12, 3, 0, &v, &f);
    ASSERT_EQ(build_sphere_wedge(&mesh, vec3(0, 0, 0), fx_from_int(10), 12, 3, 0, 0), SAT_OK);
    ASSERT_EQ(mesh.face_count, f);
    ASSERT_EQ(mesh.face_count, 36u); /* a whole sphere: no gap, no walls */
}

/* The surface faces must still point away from the centre, and the two cut
 * walls must face INTO the opening -- if either wall winds the other way it
 * is culled, and the solid renders as a sphere with a hole you can see
 * through. */
TEST(sphere_wedge_surface_and_cut_walls_face_the_right_way) {
    sat_mesh_t mesh = make_mesh();
    const sat_vec3_t center = vec3(fx_from_int(5), fx_from_int(9), fx_from_int(-3));
    const uint16_t segments = 12;
    const uint16_t rings = 3;
    const uint16_t gap_start = 2;
    const uint16_t gap = 4;
    uint16_t walls = 0;
    uint16_t i;

    ASSERT_EQ(
        build_sphere_wedge(&mesh, center, fx_from_int(12), segments, rings, gap_start, gap),
        SAT_OK);

    /* Middle of the opening, as a direction on the ground plane. Longitude 0
     * is +Z and increases towards +X, so segment k spans 30k degrees. */
    const int mid_deg = 30 * (gap_start + (gap / 2));
    const sat_vec3_t gap_dir = vec3(
        saturn::core::math3d::sin_deg_fx(fx_from_int(mid_deg)),
        0,
        saturn::core::math3d::cos_deg_fx(fx_from_int(mid_deg)));

    for (i = 0; i < mesh.face_count; ++i) {
        sat_vec3_t normal;
        sat_vec3_t fc;
        ASSERT_EQ(face_normal(&mesh, i, &normal), SAT_OK);
        ASSERT_EQ(face_center(&mesh, i, &fc), SAT_OK);
        if (vec3_dot_raw(normal, vec3_sub(fc, center)) > 0) {
            continue; /* an outward-facing surface face */
        }
        /* Everything else has to be a cut wall, facing into the opening. */
        ++walls;
        ASSERT_TRUE(vec3_dot_raw(normal, gap_dir) > 0);
    }
    ASSERT_EQ(walls, (uint16_t)(2u * rings));
}

TEST(sphere_wedge_gap_removes_the_right_bands) {
    sat_mesh_t mesh = make_mesh();
    const uint16_t segments = 12;
    uint16_t i;
    int found_in_gap = 0;

    /* A gap that wraps past longitude 0 must be handled without a special
     * case, because the direction an actor faces is not chosen to avoid it. */
    ASSERT_EQ(
        build_sphere_wedge(&mesh, vec3(0, 0, 0), fx_from_int(10), segments, 3, 11, 3),
        SAT_OK);

    for (i = 0; i < mesh.face_count; ++i) {
        sat_vec3_t fc;
        ASSERT_EQ(face_center(&mesh, i, &fc), SAT_OK);
        /* Bands 11, 0 and 1 span 330..390 degrees: centred on +Z, so a
         * surface face there would sit at positive z with small |x|. Cut
         * walls do reach into that arc, so only non-axis faces count. */
        if (fc.z > fx_from_int(6) && fx_abs(fc.x) < fx_from_int(2)) {
            found_in_gap = 1;
        }
    }
    ASSERT_FALSE(found_in_gap);
}

/* A sphere's pole and seam are duplicated once per ring/segment; welding
 * must remove exactly those duplicates and leave the winding intact. */
TEST(weld_vertices_shrinks_a_sphere_and_keeps_winding) {
    sat_mesh_t mesh = make_mesh();
    static uint16_t remap[kVertexCap];
    const sat_vec3_t center = vec3(0, 0, 0);
    uint16_t before;
    uint16_t before_faces;

    ASSERT_EQ(build_sphere(&mesh, center, fx_from_int(10), 12, 6), SAT_OK);
    before = mesh.vertex_count;
    before_faces = mesh.face_count;

    ASSERT_EQ(weld_vertices(&mesh, fx_from_int(1) / 16, remap, kVertexCap), SAT_OK);

    ASSERT_TRUE(mesh.vertex_count < before);
    ASSERT_EQ(mesh.face_count, before_faces); /* faces are untouched, only reindexed */
    for (uint16_t i = 0; i < mesh.vertex_count; ++i) {
        const sat_vec3_t d = vec3_sub(mesh.vertices[i], center);
        ASSERT_NEAR(fx_to_int(saturn::core::math3d::vec3_length(d)), 10, 1);
    }
    assert_outward(mesh, center, "welded sphere");
}

/* A quad-only mesh with no coincident vertices (every corner belongs to
 * exactly one face) must come through unchanged, not just "no crash". */
TEST(weld_vertices_leaves_a_box_alone) {
    sat_mesh_t mesh = make_mesh();
    static uint16_t remap[kVertexCap];
    const sat_vec3_t center = vec3(0, 0, 0);

    ASSERT_EQ(build_box(&mesh, center, fx_from_int(3), fx_from_int(3), fx_from_int(3)), SAT_OK);
    const uint16_t before = mesh.vertex_count;

    ASSERT_EQ(weld_vertices(&mesh, fx_from_int(1) / 16, remap, kVertexCap), SAT_OK);

    ASSERT_EQ(mesh.vertex_count, before);
    assert_outward(mesh, center, "welded box");
}

TEST(weld_vertices_rejects_undersized_scratch) {
    sat_mesh_t mesh = make_mesh();
    uint16_t remap[4];

    ASSERT_EQ(build_box(&mesh, vec3(0, 0, 0), 1, 1, 1), SAT_OK); /* 8 vertices */
    ASSERT_EQ(weld_vertices(&mesh, fx_from_int(1) / 16, remap, 4u), SAT_ERR_CAPACITY);
    ASSERT_EQ(mesh.vertex_count, 8u); /* rejected atomically, nothing touched */
}

/* Orienting a primitive after building it is how a sphere's mouth ends up
 * opening up-and-down instead of side-to-side: build in the canonical pose,
 * then rotate. A rotation must move the vertices and leave the shape alone. */
TEST(transform_rotates_a_built_primitive) {
    sat_mesh_t mesh = make_mesh();
    sat_mat4_t rot;
    sat_vec3_t before;
    sat_vec3_t after;

    ASSERT_EQ(build_box(&mesh, vec3(0, 0, 0), fx_from_int(4), fx_from_int(10), fx_from_int(4)),
              SAT_OK);
    ASSERT_EQ(face_normal(&mesh, 4, &before), SAT_OK); /* the +Y face */
    ASSERT_NEAR(before.y, SAT_FX16_ONE, 64);

    /* Rotating 90 degrees about Z takes +Y to -X. */
    ASSERT_EQ(sat_mat4_rotate_z(&rot, fx_from_int(90)), SAT_OK);
    ASSERT_EQ(transform(&mesh, rot.m), SAT_OK);
    ASSERT_EQ(face_normal(&mesh, 4, &after), SAT_OK);
    ASSERT_NEAR(after.x, -SAT_FX16_ONE, 256);
    ASSERT_NEAR(after.y, 0, 256);

    /* The box is still a box: its half extents have swapped, not changed. */
    sat_fx16_t max_x = 0;
    sat_fx16_t max_y = 0;
    for (uint16_t i = 0; i < mesh.vertex_count; ++i) {
        const sat_fx16_t ax = fx_abs(mesh.vertices[i].x);
        const sat_fx16_t ay = fx_abs(mesh.vertices[i].y);
        if (ax > max_x) { max_x = ax; }
        if (ay > max_y) { max_y = ay; }
    }
    ASSERT_NEAR(fx_to_int(max_x), 10, 1);
    ASSERT_NEAR(fx_to_int(max_y), 4, 1);
}

TEST(null_texture_table_selects_polygon_for_every_face) {
    uint16_t out = 0xBEEFu;
    ASSERT_TRUE(resolve_face_draw_mode(0, nullptr, 4, nullptr, 0, &out) ==
                mesh_face_draw_mode::kPolygon);
    ASSERT_TRUE(resolve_face_draw_mode(3, nullptr, 4, nullptr, 0, nullptr) ==
                mesh_face_draw_mode::kPolygon);
    ASSERT_EQ(validate_face_textures(nullptr, 4, nullptr, 0), SAT_OK);
}

TEST(texture_none_selects_polygon_path) {
    sat_vdp1_texture_t tex[1] = {};
    const uint16_t table[2] = {SAT_MESH_TEXTURE_NONE, SAT_MESH_TEXTURE_NONE};
    uint16_t out = 0xBEEFu;
    ASSERT_TRUE(resolve_face_draw_mode(0, table, 2, tex, 1, &out) ==
                mesh_face_draw_mode::kPolygon);
    ASSERT_EQ(validate_face_textures(table, 2, tex, 1), SAT_OK);
    /* A table of NONE needs no texture storage to be valid. */
    ASSERT_EQ(validate_face_textures(table, 2, nullptr, 0), SAT_OK);
}

TEST(valid_texture_index_resolves_to_textured) {
    sat_vdp1_texture_t tex[2] = {};
    const uint16_t table[3] = {0u, 1u, SAT_MESH_TEXTURE_NONE};
    uint16_t out = 0xBEEFu;
    ASSERT_TRUE(resolve_face_draw_mode(0, table, 3, tex, 2, &out) ==
                mesh_face_draw_mode::kTextured);
    ASSERT_EQ(out, 0u);
    ASSERT_TRUE(resolve_face_draw_mode(1, table, 3, tex, 2, &out) ==
                mesh_face_draw_mode::kTextured);
    ASSERT_EQ(out, 1u);
    ASSERT_TRUE(resolve_face_draw_mode(2, table, 3, tex, 2, &out) ==
                mesh_face_draw_mode::kPolygon);
    ASSERT_EQ(validate_face_textures(table, 3, tex, 2), SAT_OK);
}

TEST(out_of_range_texture_index_is_invalid) {
    sat_vdp1_texture_t tex[1] = {};
    const uint16_t table[2] = {0u, 5u};
    ASSERT_TRUE(resolve_face_draw_mode(1, table, 2, tex, 1, nullptr) ==
                mesh_face_draw_mode::kInvalid);
    ASSERT_EQ(validate_face_textures(table, 2, tex, 1), SAT_ERR_INVALID_ARG);
    /* Null storage cannot back a real index. */
    const uint16_t table2[1] = {0u};
    ASSERT_TRUE(resolve_face_draw_mode(0, table2, 1, nullptr, 0, nullptr) ==
                mesh_face_draw_mode::kInvalid);
    ASSERT_EQ(validate_face_textures(table2, 1, nullptr, 0), SAT_ERR_INVALID_ARG);
    /* Face past the end of the mesh is invalid, not polygon. */
    ASSERT_TRUE(resolve_face_draw_mode(7, table2, 1, nullptr, 0, nullptr) ==
                mesh_face_draw_mode::kInvalid);
}

TEST(texture_selection_shares_culling_with_polygon_path) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(
        build_box(&mesh, vec3(0, 0, 0), fx_from_int(10), fx_from_int(10), fx_from_int(10)),
        SAT_OK);
    const sat_vec3_t eye = vec3(fx_from_int(0), fx_from_int(0), fx_from_int(100));
    /* Culling is decided from geometry alone; the texture table must not
     * change which faces are visible. */
    for (uint16_t i = 0; i < mesh.face_count; ++i) {
        const bool visible = face_visible(&mesh, i, eye);
        sat_quad3_t quad;
        ASSERT_EQ(face_quad(&mesh, i, &quad), SAT_OK);
        ASSERT_EQ(quad_visible(quad, eye) ? true : false, visible);
    }
}

TEST(degenerate_triangle_quad_keeps_outward_normal) {
    sat_mesh_t mesh = make_mesh();
    /* Triangle in the z=0 plane facing +Z, carried as A,B,C,C. With
     * A=(0,0), B=(0,16), C=(16,0): D-A=(16,0), B-A=(0,16), and
     * cross(D-A, B-A).z = 16*16 - 0*0 > 0. */
    uint16_t a = 0, b = 0, c = 0;
    ASSERT_EQ(add_vertex(&mesh, 0, 0, 0, &a), SAT_OK);
    ASSERT_EQ(add_vertex(&mesh, 0, fx_from_int(16), 0, &b), SAT_OK);
    ASSERT_EQ(add_vertex(&mesh, fx_from_int(16), 0, 0, &c), SAT_OK);
    ASSERT_EQ(add_face(&mesh, a, b, c, c), SAT_OK);
    sat_vec3_t normal;
    ASSERT_EQ(face_normal_scaled(&mesh, 0, &normal), SAT_OK);
    ASSERT_TRUE(normal.z > 0);
    const sat_vec3_t eye = vec3(0, 0, fx_from_int(100));
    ASSERT_TRUE(face_visible(&mesh, 0, eye));
    ASSERT_FALSE(face_visible(&mesh, 0, vec3(0, 0, fx_from_int(-100))));
}

TEST(octahedron_counts_match_builder_and_wind_outward) {
    sat_mesh_t mesh = make_mesh();
    const sat_vec3_t center = vec3(fx_from_int(9), fx_from_int(-3), fx_from_int(17));
    uint16_t vertices = 0, faces = 0;
    octahedron_counts(&vertices, &faces);
    ASSERT_EQ(vertices, 6u);
    ASSERT_EQ(faces, 8u);
    ASSERT_EQ(build_octahedron(&mesh, center, fx_from_int(3), fx_from_int(5)), SAT_OK);
    ASSERT_EQ(mesh.vertex_count, vertices);
    ASSERT_EQ(mesh.face_count, faces);
    ASSERT_EQ(mesh.vertices[0].x, center.x + fx_from_int(3));
    ASSERT_EQ(mesh.vertices[1].z, center.z + fx_from_int(3));
    ASSERT_EQ(mesh.vertices[4].y, center.y + fx_from_int(5));
    ASSERT_EQ(mesh.vertices[5].y, center.y - fx_from_int(5));
    assert_outward(mesh, center, "octahedron");
}

TEST(octahedron_faces_are_interleaved_triangle_pairs) {
    sat_mesh_t mesh = make_mesh();
    ASSERT_EQ(build_octahedron(&mesh, vec3(0, 0, 0), fx_from_int(2),
                               fx_from_int(4)), SAT_OK);
    for (uint16_t side = 0u; side < 4u; ++side) {
        const uint16_t next = static_cast<uint16_t>((side + 1u) & 3u);
        const uint16_t upper = static_cast<uint16_t>(side * 8u);
        const uint16_t lower = static_cast<uint16_t>(upper + 4u);
        ASSERT_EQ(mesh.indices[upper], 4u);
        ASSERT_EQ(mesh.indices[upper + 1u], side);
        ASSERT_EQ(mesh.indices[upper + 2u], next);
        ASSERT_EQ(mesh.indices[upper + 3u], next);
        ASSERT_EQ(mesh.indices[lower], side);
        ASSERT_EQ(mesh.indices[lower + 1u], 5u);
        ASSERT_EQ(mesh.indices[lower + 2u], next);
        ASSERT_EQ(mesh.indices[lower + 3u], next);
    }
}

TEST(octahedron_capacity_is_atomic) {
    sat_vec3_t vertices[5];
    uint16_t indices[8u * 4u];
    sat_mesh_t mesh;
    ASSERT_EQ(init(&mesh, vertices, 5u, indices, 8u), SAT_OK);
    ASSERT_EQ(add_vertex(&mesh, 11, 22, 33, nullptr), SAT_OK);
    ASSERT_EQ(build_octahedron(&mesh, vec3(0, 0, 0), fx_from_int(2),
                               fx_from_int(3)), SAT_ERR_CAPACITY);
    ASSERT_EQ(mesh.vertex_count, 1u);
    ASSERT_EQ(mesh.vertices[0].x, 11);
    ASSERT_EQ(mesh.face_count, 0u);
}

TEST(octahedron_rejects_invalid_extents_without_modifying_mesh) {
    sat_mesh_t mesh = make_mesh();
    const sat_vec3_t center = vec3(0, 0, 0);
    ASSERT_EQ(build_octahedron(&mesh, center, fx_from_int(2),
                               fx_from_int(3)), SAT_OK);
    ASSERT_EQ(build_octahedron(&mesh, center, 0, fx_from_int(3)),
              SAT_ERR_INVALID_ARG);
    ASSERT_EQ(build_octahedron(&mesh, center, fx_from_int(2), -1),
              SAT_ERR_INVALID_ARG);
    ASSERT_EQ(mesh.vertex_count, 6u);
    ASSERT_EQ(mesh.face_count, 8u);
    ASSERT_EQ(build_octahedron(nullptr, center, 1, 1), SAT_ERR_INVALID_ARG);
}

int main() {
    octahedron_counts_match_builder_and_wind_outward();
    octahedron_faces_are_interleaved_triangle_pairs();
    octahedron_capacity_is_atomic();
    octahedron_rejects_invalid_extents_without_modifying_mesh();
    init_rejects_null_storage();
    add_face_rejects_unknown_vertices();
    add_vertex_reports_capacity();
    add_quad_appends_four_vertices_and_one_face();
    counts_match_what_the_builders_produce();
    box_faces_wind_outward();
    long_thin_box_still_winds_outward();
    plane_normal_points_up();
    plane_spans_the_requested_extent();
    sphere_faces_wind_outward();
    sphere_vertices_sit_on_the_radius();
    sphere_clamps_degenerate_subdivision();
    cylinder_faces_wind_outward();
    uncapped_cylinder_has_only_side_faces();
    builders_report_capacity_without_corrupting_the_mesh();
    face_visible_matches_which_side_the_eye_is_on();
    translate_moves_every_vertex();
    clear_keeps_the_storage_binding();
    scaled_and_unit_normals_point_the_same_way();
    sphere_wedge_counts_match_the_builder();
    sphere_wedge_surface_and_cut_walls_face_the_right_way();
    sphere_wedge_gap_removes_the_right_bands();
    transform_rotates_a_built_primitive();
    null_texture_table_selects_polygon_for_every_face();
    texture_none_selects_polygon_path();
    valid_texture_index_resolves_to_textured();
    out_of_range_texture_index_is_invalid();
    texture_selection_shares_culling_with_polygon_path();
    degenerate_triangle_quad_keeps_outward_normal();
    weld_vertices_shrinks_a_sphere_and_keeps_winding();
    weld_vertices_leaves_a_box_alone();
    weld_vertices_rejects_undersized_scratch();
    printf("test_mesh3d_logic: 36 tests passed\n");
    return 0;
}
