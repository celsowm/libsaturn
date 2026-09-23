/* test_math3d_logic.cpp — host tests for the 3D math module */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/math3d.h"
#include "src/core/math3d/logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)
#define ASSERT_NEAR(a, b, tol) do { \
    long long _d = (long long)(a) - (long long)(b); \
    if (_d < 0) _d = -_d; \
    if (_d > (tol)) { \
        fprintf(stderr, "FAIL %s:%d: %s (%ld) not within %ld of %s (%ld)\n", \
                __FILE__, __LINE__, #a, (long)(a), (long)(tol), #b, (long)(b)); \
        exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

using namespace saturn::core::math3d;

/* fx_sqrt is the exact floor root of v << 16 across the whole positive range. */
TEST(fx_sqrt_is_floor_root) {
    const uint32_t extremes[] = {1u, 2u, 3u, 65535u, 65536u, 65537u, 0x7FFFFFFFu};
    for (uint32_t i = 0; i < sizeof(extremes) / sizeof(extremes[0]) + 5000u; ++i) {
        const uint32_t v = (i < sizeof(extremes) / sizeof(extremes[0]))
            ? extremes[i]
            : (uint32_t)((i * 2654435761u) & 0x7FFFFFFFu) | 1u;
        const uint64_t x = (uint64_t)v << 16;
        const uint64_t r = (uint64_t)fx_sqrt((sat_fx16_t)v);
        ASSERT_TRUE(r * r <= x);
        ASSERT_TRUE((r + 1) * (r + 1) > x);
    }
}

TEST(fx_mul_and_div_roundtrip) {
    const sat_fx16_t two = fx_from_int(2);
    const sat_fx16_t three = fx_from_int(3);
    ASSERT_EQ(fx_mul(two, three), fx_from_int(6));
    ASSERT_EQ(fx_div(fx_from_int(6), three), two);
}

TEST(fx_sqrt_exact) {
    ASSERT_EQ(fx_sqrt(fx_from_int(9)), fx_from_int(3));
    ASSERT_EQ(fx_sqrt(0), 0);
    ASSERT_EQ(fx_sqrt(-1), 0);
    ASSERT_NEAR(fx_sqrt(fx_from_int(2)), 92681, 64);  /* sqrt(2) ~= 1.41421 */
}

TEST(sin_cos_cardinal_values) {
    ASSERT_NEAR(sin_deg_fx(0), 0, 64);
    ASSERT_NEAR(sin_deg_fx(fx_from_int(90)), SAT_FX16_ONE, 256);
    ASSERT_NEAR(sin_deg_fx(fx_from_int(180)), 0, 256);
    ASSERT_NEAR(sin_deg_fx(fx_from_int(270)), -SAT_FX16_ONE, 256);
    ASSERT_NEAR(cos_deg_fx(0), SAT_FX16_ONE, 256);
    ASSERT_NEAR(cos_deg_fx(fx_from_int(180)), -SAT_FX16_ONE, 256);
}

TEST(sin_45_degrees) {
    /* sin(45) = sqrt(2)/2 ~= 0.70711 => 46341 in fx16. */
    ASSERT_NEAR(sin_deg_fx(fx_from_int(45)), 46341, 512);
}

TEST(mat4_identity_multiply) {
    sat_mat4_t id;
    ASSERT_EQ(sat_mat4_identity(&id), SAT_OK);
    sat_mat4_t out;
    ASSERT_EQ(sat_mat4_multiply(&out, &id, &id), SAT_OK);
    for (int i = 0; i < 16; ++i) {
        ASSERT_EQ(out.m[i], id.m[i]);
    }
}

TEST(mat4_translate_transform) {
    sat_mat4_t t;
    ASSERT_EQ(sat_mat4_translate(&t, fx_from_int(5), fx_from_int(0), fx_from_int(0)), SAT_OK);
    sat_vec4_t v = { fx_from_int(1), 0, 0, SAT_FX16_ONE };
    sat_vec4_t out;
    ASSERT_EQ(sat_mat4_transform_vec4(&t, &v, &out), SAT_OK);
    ASSERT_EQ(out.x, fx_from_int(6));
    ASSERT_EQ(out.y, 0);
    ASSERT_EQ(out.z, 0);
}

TEST(mat4_rotate_z_90) {
    sat_mat4_t r;
    ASSERT_EQ(sat_mat4_rotate_z(&r, fx_from_int(90)), SAT_OK);
    sat_vec4_t v = { SAT_FX16_ONE, 0, 0, SAT_FX16_ONE };
    sat_vec4_t out;
    ASSERT_EQ(sat_mat4_transform_vec4(&r, &v, &out), SAT_OK);
    ASSERT_NEAR(out.x, 0, 64);
    ASSERT_NEAR(out.y, SAT_FX16_ONE, 64);
    ASSERT_EQ(out.z, 0);
}

TEST(mat4_look_at_basic) {
    sat_mat4_t view;
    sat_vec3_t eye = { fx_from_int(0), fx_from_int(0), fx_from_int(1) };
    sat_vec3_t center = { 0, 0, 0 };
    sat_vec3_t up = { 0, 1, 0 };
    ASSERT_EQ(sat_mat4_look_at(&view, &eye, &center, &up), SAT_OK);
    /* The look-at target is at world (0,0,0). From the camera at (0,0,1)
     * looking toward the origin, that point is 1 unit in front of the camera,
     * i.e. at view-space z = -1. */
    sat_vec4_t v = { 0, 0, 0, SAT_FX16_ONE };
    sat_vec4_t out;
    ASSERT_EQ(sat_mat4_transform_vec4(&view, &v, &out), SAT_OK);
    ASSERT_NEAR(out.z, -SAT_FX16_ONE, 64);
}

TEST(mat4_perspective_basic) {
    sat_mat4_t proj;
    ASSERT_EQ(sat_mat4_perspective(&proj, fx_from_int(90), fx_from_int(4) / fx_from_int(3),
                                    fx_from_int(1), fx_from_int(100)), SAT_OK);
    /* With 90-degree FOV, aspect 4/3, near 1, far 100:
     * projection of (0, 0, -1, 1) (a point at near plane) should give ndc (0,0) */
    sat_vec4_t v = { 0, 0, -SAT_FX16_ONE, SAT_FX16_ONE };
    sat_vec4_t out;
    ASSERT_EQ(sat_mat4_transform_vec4(&proj, &v, &out), SAT_OK);
    /* After perspective divide, ndc should be (0, 0) */
    ASSERT_NEAR(out.x, 0, 64);
    ASSERT_NEAR(out.y, 0, 64);
}

TEST(project_to_screen_basic) {
    sat_mat4_t view, proj;
    ASSERT_EQ(sat_mat4_identity(&view), SAT_OK);
    view.m[14] = -SAT_FX16_ONE;  /* translate z by -1 */
    ASSERT_EQ(sat_mat4_perspective(&proj, fx_from_int(90), fx_from_int(1),
                                    fx_from_int(1), fx_from_int(100)), SAT_OK);
    sat_mat4_t vp;
    ASSERT_EQ(sat_mat4_multiply(&vp, &proj, &view), SAT_OK);

    int16_t sx, sy;
    ASSERT_EQ(sat_project_to_screen(&vp, 0, 0, -SAT_FX16_ONE, 320, 224, &sx, &sy), SAT_OK);
    ASSERT_EQ(sx, 160);
    ASSERT_EQ(sy, 112);
}

TEST(project_to_screen_behind_camera) {
    /* Camera at origin looking toward -Z (default OpenGL convention).
     * A world point at (0,0,+1) is behind the camera, so projection must
     * refuse to rasterise it. */
    sat_mat4_t view, proj, vp;
    sat_vec3_t eye = { 0, 0, 0 };
    sat_vec3_t center = { 0, 0, -SAT_FX16_ONE };
    sat_vec3_t up = { 0, SAT_FX16_ONE, 0 };
    ASSERT_EQ(sat_mat4_look_at(&view, &eye, &center, &up), SAT_OK);
    ASSERT_EQ(sat_mat4_perspective(&proj, fx_from_int(90), fx_from_int(1),
                                    fx_from_int(1), fx_from_int(100)), SAT_OK);
    ASSERT_EQ(sat_mat4_multiply(&vp, &proj, &view), SAT_OK);
    int16_t sx, sy;
    ASSERT_EQ(sat_project_to_screen(&vp, 0, 0, SAT_FX16_ONE, 320, 224, &sx, &sy),
              SAT_ERR_UNSUPPORTED);
}

TEST(mat4_scale_transform) {
    sat_mat4_t s;
    ASSERT_EQ(sat_mat4_scale(&s, fx_from_int(2), fx_from_int(2), fx_from_int(2)), SAT_OK);
    sat_vec4_t v = { fx_from_int(1), fx_from_int(1), fx_from_int(1), SAT_FX16_ONE };
    sat_vec4_t out;
    ASSERT_EQ(sat_mat4_transform_vec4(&s, &v, &out), SAT_OK);
    ASSERT_EQ(out.x, fx_from_int(2));
    ASSERT_EQ(out.y, fx_from_int(2));
    ASSERT_EQ(out.z, fx_from_int(2));
}


TEST(vec3_dot_and_cross_follow_the_right_hand_rule) {
    sat_vec3_t x = { fx_from_int(1), 0, 0 };
    sat_vec3_t y = { 0, fx_from_int(1), 0 };
    sat_vec3_t out;
    ASSERT_EQ(sat_vec3_dot(&x, &y), 0);
    ASSERT_EQ(sat_vec3_dot(&x, &x), SAT_FX16_ONE);
    sat_vec3_cross(&out, &x, &y);
    ASSERT_EQ(out.x, 0);
    ASSERT_EQ(out.y, 0);
    ASSERT_EQ(out.z, SAT_FX16_ONE);
}

TEST(vec3_normalize_leaves_a_zero_vector_alone) {
    sat_vec3_t zero = { 0, 0, 0 };
    sat_vec3_t out = { fx_from_int(9), fx_from_int(9), fx_from_int(9) };
    sat_vec3_normalize(&out, &zero);
    ASSERT_EQ(out.x, 0);
    ASSERT_EQ(out.y, 0);
    ASSERT_EQ(out.z, 0);
}

TEST(vec3_normalize_gives_unit_length) {
    sat_vec3_t v = { fx_from_int(3), fx_from_int(4), 0 };
    sat_vec3_t out;
    ASSERT_EQ(sat_vec3_length(&v), fx_from_int(5));
    sat_vec3_normalize(&out, &v);
    /* 16.16 division leaves a bit or two of slack. */
    const sat_fx16_t len = sat_vec3_length(&out);
    ASSERT_TRUE(len > SAT_FX16_ONE - 128 && len < SAT_FX16_ONE + 128);
}

/* vec3_cross reduces by 2^16 as it goes and overflows on long edges;
 * vec3_cross_unit keeps the full products. A wall run crossed with its own
 * height is exactly the case that broke. */
TEST(vec3_cross_unit_survives_long_edges) {
    const sat_vec3_t along = { fx_from_int(224), 0, 0 };
    const sat_vec3_t down = { 0, fx_from_int(-200), 0 };
    const sat_vec3_t n = saturn::core::math3d::vec3_cross_unit(down, along);
    ASSERT_EQ(n.x, 0);
    ASSERT_EQ(n.y, 0);
    ASSERT_TRUE(n.z > SAT_FX16_ONE - 128);
}


/* fx_mul(v, v) overflows int32 once v passes about 181.0, so building a view
 * matrix from a camera further out than that used to produce a sign-flipped
 * forward vector and a view of nothing in particular. A board-game camera is
 * routinely 250+ units up. */
TEST(look_at_survives_a_distant_camera) {
    sat_mat4_t view;
    const sat_vec3_t eye = { fx_from_int(112), fx_from_int(260), fx_from_int(290) };
    const sat_vec3_t center = { fx_from_int(112), 0, fx_from_int(100) };
    const sat_vec3_t up = { 0, SAT_FX16_ONE, 0 };
    ASSERT_EQ(sat_mat4_look_at(&view, &eye, &center, &up), SAT_OK);

    /* Row 2 is -forward, so it must be a unit vector pointing back towards
     * the camera: up and away from the board, never negative. */
    ASSERT_TRUE(view.m[9] > 0);
    ASSERT_TRUE(view.m[10] > 0);
    const sat_vec3_t back = { view.m[8], view.m[9], view.m[10] };
    const sat_fx16_t len = sat_vec3_length(&back);
    ASSERT_TRUE(len > SAT_FX16_ONE - 256 && len < SAT_FX16_ONE + 256);
}

TEST(vec3_length_does_not_overflow_on_long_vectors) {
    const sat_vec3_t v = { fx_from_int(3000), fx_from_int(4000), 0 };
    ASSERT_NEAR(sat_vec3_length(&v), fx_from_int(5000), 256);
}

TEST(fx16_abs_negates_only_negative_values) {
    ASSERT_EQ(sat_fx16_abs(fx_from_int(5)), fx_from_int(5));
    ASSERT_EQ(sat_fx16_abs(fx_from_int(-5)), fx_from_int(5));
    ASSERT_EQ(sat_fx16_abs(0), 0);
    ASSERT_EQ(sat_fx16_abs(1), 1);
    ASSERT_EQ(sat_fx16_abs(-1), 1);
}

int main() {
    fx_mul_and_div_roundtrip();
    fx_sqrt_exact();
    fx_sqrt_is_floor_root();
    sin_cos_cardinal_values();
    sin_45_degrees();
    mat4_identity_multiply();
    mat4_translate_transform();
    mat4_rotate_z_90();
    mat4_look_at_basic();
    mat4_perspective_basic();
    project_to_screen_basic();
    project_to_screen_behind_camera();
    mat4_scale_transform();
    vec3_dot_and_cross_follow_the_right_hand_rule();
    vec3_normalize_leaves_a_zero_vector_alone();
    vec3_normalize_gives_unit_length();
    vec3_cross_unit_survives_long_edges();
    look_at_survives_a_distant_camera();
    vec3_length_does_not_overflow_on_long_vectors();
    fx16_abs_negates_only_negative_values();

    printf("PASS: test_math3d_logic.cpp (%d tests)\n", 20);
    return 0;
}
