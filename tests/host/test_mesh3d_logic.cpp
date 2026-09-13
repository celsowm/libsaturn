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

int main() {
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
    printf("test_mesh3d_logic: 18 tests passed\n");
    return 0;
}
