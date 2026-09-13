/* test_render3d_logic.cpp — host tests for world-quad projection and shading */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/math3d.h"
#include "saturn/color.h"
#include "saturn/render3d.h"
#include "src/core/render3d_logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
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
#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

using namespace saturn::core::render3d;
using saturn::core::math3d::fx_from_int;
using saturn::core::math3d::fx_to_int;

static const int16_t kW = 320;
static const int16_t kH = 224;

/* Camera at the origin looking down -Z, which is what sat_mat4_look_at
 * produces for a right-handed view. */
static sat_mat4_t make_view_proj(int eye_z) {
    sat_mat4_t view;
    sat_mat4_t proj;
    sat_mat4_t vp;
    sat_vec3_t eye = { 0, 0, fx_from_int(eye_z) };
    sat_vec3_t center = { 0, 0, 0 };
    sat_vec3_t up = { 0, SAT_FX16_ONE, 0 };
    sat_mat4_look_at(&view, &eye, &center, &up);
    sat_mat4_perspective(
        &proj,
        fx_from_int(90),
        sat_fx16_div(fx_from_int(kW), fx_from_int(kH)),
        fx_from_int(1),
        fx_from_int(500));
    sat_mat4_multiply(&vp, &proj, &view);
    return vp;
}

TEST(wall_quad_corner_order) {
    sat_quad3_t q;
    make_wall(&q, fx_from_int(0), fx_from_int(8), fx_from_int(16), fx_from_int(8),
              fx_from_int(24));
    /* A and B on top, C and D on the floor. */
    ASSERT_EQ(q.v[0].y, fx_from_int(24));
    ASSERT_EQ(q.v[1].y, fx_from_int(24));
    ASSERT_EQ(q.v[2].y, 0);
    ASSERT_EQ(q.v[3].y, 0);
    /* A/D share the start of the segment, B/C the end. */
    ASSERT_EQ(q.v[0].x, fx_from_int(0));
    ASSERT_EQ(q.v[3].x, fx_from_int(0));
    ASSERT_EQ(q.v[1].x, fx_from_int(16));
    ASSERT_EQ(q.v[2].x, fx_from_int(16));
    ASSERT_EQ(q.v[0].z, fx_from_int(8));
    ASSERT_EQ(q.v[2].z, fx_from_int(8));
}

TEST(floor_quad_is_flat_and_centred) {
    sat_quad3_t q;
    make_floor(&q, fx_from_int(20), fx_from_int(1), fx_from_int(30), fx_from_int(4));
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(q.v[i].y, fx_from_int(1));
    }
    ASSERT_EQ(q.v[0].x, fx_from_int(16));
    ASSERT_EQ(q.v[1].x, fx_from_int(24));
    ASSERT_EQ(q.v[0].z, fx_from_int(26));
    ASSERT_EQ(q.v[2].z, fx_from_int(34));
}

TEST(billboard_spreads_along_right_vector) {
    sat_quad3_t q;
    /* Right vector pointing down +X: the panel spreads in X and stays at cz. */
    make_billboard(&q, fx_from_int(10), fx_from_int(50), SAT_FX16_ONE, 0,
                   fx_from_int(6), fx_from_int(18));
    ASSERT_EQ(q.v[0].x, fx_from_int(4));
    ASSERT_EQ(q.v[1].x, fx_from_int(16));
    ASSERT_EQ(q.v[0].z, fx_from_int(50));
    ASSERT_EQ(q.v[1].z, fx_from_int(50));
    ASSERT_EQ(q.v[0].y, fx_from_int(18));
    ASSERT_EQ(q.v[2].y, 0);

    /* Right vector pointing down +Z: the panel spreads in Z instead. */
    make_billboard(&q, fx_from_int(10), fx_from_int(50), 0, SAT_FX16_ONE,
                   fx_from_int(6), fx_from_int(18));
    ASSERT_EQ(q.v[0].z, fx_from_int(44));
    ASSERT_EQ(q.v[1].z, fx_from_int(56));
    ASSERT_EQ(q.v[0].x, fx_from_int(10));
}

/* A point on the view axis must land on the native origin, i.e. the screen
 * centre -- this is the assertion that catches a missing or doubled
 * half-screen offset. */
TEST(project_axis_point_to_native_origin) {
    const sat_mat4_t vp = make_view_proj(100);
    int16_t x = 999;
    int16_t y = 999;
    ASSERT_TRUE(project_native(vp.m, 0, 0, 0, kW, kH, &x, &y));
    ASSERT_NEAR(x, 0, 1);
    ASSERT_NEAR(y, 0, 1);
}

TEST(project_respects_screen_axes) {
    const sat_mat4_t vp = make_view_proj(100);
    int16_t x = 0;
    int16_t y = 0;
    /* +X in world is right on screen. */
    ASSERT_TRUE(project_native(vp.m, fx_from_int(20), 0, 0, kW, kH, &x, &y));
    ASSERT_TRUE(x > 0);
    ASSERT_NEAR(y, 0, 1);
    /* +Y in world is UP, which is negative in native screen coordinates. */
    ASSERT_TRUE(project_native(vp.m, 0, fx_from_int(20), 0, kW, kH, &x, &y));
    ASSERT_TRUE(y < 0);
    ASSERT_NEAR(x, 0, 1);
}

TEST(project_rejects_points_behind_camera) {
    const sat_mat4_t vp = make_view_proj(100);
    int16_t x = 0;
    int16_t y = 0;
    /* The camera sits at z = 100 looking toward -Z, so z = 200 is behind it. */
    ASSERT_FALSE(project_native(vp.m, 0, 0, fx_from_int(200), kW, kH, &x, &y));
}

/* The VDP1 cannot clip, so a quad straddling the camera plane has to be
 * dropped whole rather than drawn folded across the screen. */
TEST(project_quad_rejects_straddling_quads) {
    const sat_mat4_t vp = make_view_proj(100);
    sat_quad2_t out;

    sat_quad3_t front;
    make_wall(&front, fx_from_int(-10), fx_from_int(0), fx_from_int(10),
              fx_from_int(0), fx_from_int(20));
    ASSERT_TRUE(project_quad(vp.m, &front, kW, kH, &out));

    sat_quad3_t straddling;
    make_wall(&straddling, fx_from_int(-10), fx_from_int(0), fx_from_int(10),
              fx_from_int(300), fx_from_int(20));
    ASSERT_FALSE(project_quad(vp.m, &straddling, kW, kH, &out));
}

/* A point very close to the camera plane projects enormous; it must clamp
 * rather than wrap its sign and turn the quad inside out. */
TEST(project_clamps_instead_of_wrapping) {
    const sat_mat4_t vp = make_view_proj(100);
    int16_t x = 0;
    int16_t y = 0;
    ASSERT_TRUE(project_native(
        vp.m, fx_from_int(4000), 0, fx_from_int(99), kW, kH, &x, &y));
    ASSERT_EQ(x, (int16_t)kCoordLimit);

    ASSERT_TRUE(project_native(
        vp.m, fx_from_int(-4000), 0, fx_from_int(99), kW, kH, &x, &y));
    ASSERT_EQ(x, (int16_t)-kCoordLimit);
}

TEST(project_quad_rejects_null_arguments) {
    const sat_mat4_t vp = make_view_proj(100);
    sat_quad3_t q;
    sat_quad2_t out;
    make_floor(&q, 0, 0, 0, fx_from_int(4));
    ASSERT_FALSE(project_quad(nullptr, &q, kW, kH, &out));
    ASSERT_FALSE(project_quad(vp.m, nullptr, kW, kH, &out));
    ASSERT_FALSE(project_quad(vp.m, &q, kW, kH, nullptr));
}

TEST(sort_orders_far_to_near) {
    uint8_t idx[5] = {0, 1, 2, 3, 4};
    const uint32_t keys[5] = {10u, 400u, 50u, 400u, 1u};
    sort_indices_desc(idx, keys, 5u);
    for (int i = 1; i < 5; ++i) {
        ASSERT_TRUE(keys[idx[i - 1]] >= keys[idx[i]]);
    }
    ASSERT_EQ(keys[idx[0]], 400u);
    ASSERT_EQ(keys[idx[4]], 1u);
    /* Equal keys keep their original relative order. */
    ASSERT_EQ(idx[0], 1);
    ASSERT_EQ(idx[1], 3);
}

TEST(sort_handles_degenerate_counts) {
    uint8_t idx[1] = {0};
    const uint32_t keys[1] = {7u};
    sort_indices_desc(idx, keys, 0u);
    ASSERT_EQ(idx[0], 0);
    sort_indices_desc(idx, keys, 1u);
    ASSERT_EQ(idx[0], 0);
    sort_indices_desc(nullptr, keys, 1u);
    sort_indices_desc(idx, nullptr, 1u);
}

TEST(ground_distance_saturates) {
    ASSERT_EQ(ground_distance_sq(0, 0, fx_from_int(3), fx_from_int(4)), 25u);
    ASSERT_EQ(ground_distance_sq(fx_from_int(10), fx_from_int(10),
                                 fx_from_int(10), fx_from_int(10)), 0u);
    /* Far enough apart to overflow a 32-bit square. 64000^2 * 2 needs 34
     * bits; these are the largest magnitudes sat_fx16_t can even hold, so
     * this is the real ceiling rather than a contrived one. */
    ASSERT_EQ(ground_distance_sq(fx_from_int(-32000), fx_from_int(-32000),
                                 fx_from_int(32000), fx_from_int(32000)),
              0xFFFFFFFFu);
}

/* Saturn color words are BGR555 (red low, blue high). Cross-checking the
 * macro against the named constants is what keeps a channel swap -- which
 * produces a picture that looks fine but is the wrong colour -- from going
 * unnoticed. */
TEST(rgb555_macro_matches_named_colors) {
    ASSERT_EQ(SAT_RGB555(31, 0, 0), (uint16_t)(SAT_COLOR_RED | 0x8000u));
    ASSERT_EQ(SAT_RGB555(0, 31, 0), (uint16_t)(SAT_COLOR_GREEN | 0x8000u));
    ASSERT_EQ(SAT_RGB555(0, 0, 31), (uint16_t)(SAT_COLOR_BLUE | 0x8000u));
    ASSERT_EQ(SAT_RGB555(31, 31, 0), (uint16_t)(SAT_COLOR_YELLOW | 0x8000u));
    ASSERT_EQ(SAT_RGB555(0, 31, 31), (uint16_t)(SAT_COLOR_CYAN | 0x8000u));
    ASSERT_EQ(SAT_RGB555(31, 0, 31), (uint16_t)(SAT_COLOR_MAGENTA | 0x8000u));
    ASSERT_EQ(SAT_RGB555(31, 31, 31), (uint16_t)(SAT_COLOR_WHITE | 0x8000u));
    ASSERT_EQ(SAT_BGR555(31, 0, 0), (uint16_t)SAT_COLOR_RED);
    /* Components are masked, so an out-of-range value cannot bleed into the
     * neighbouring channel. */
    ASSERT_EQ(SAT_RGB555(32, 0, 0), (uint16_t)0x8000u);
}

TEST(shade_scales_channels) {
    const uint16_t white = SAT_RGB555(31, 31, 31);
    ASSERT_EQ(shade_rgb555(white, SAT_FX16_ONE), white);
    ASSERT_EQ(shade_rgb555(white, 0), SAT_RGB555(0, 0, 0));
    ASSERT_EQ(shade_rgb555(white, SAT_FX16_ONE / 2), SAT_RGB555(15, 15, 15));
    /* Negative intensity is treated as zero, not as a huge unsigned value. */
    ASSERT_EQ(shade_rgb555(white, -SAT_FX16_ONE), SAT_RGB555(0, 0, 0));
}

TEST(shade_preserves_rgb_code_bit_and_saturates) {
    const uint16_t blue = SAT_RGB555(4, 8, 30);
    const uint16_t bright = shade_rgb555(blue, fx_from_int(4));
    ASSERT_EQ(bright & 0x8000u, 0x8000u);
    /* Blue is 30; 30 * 4 clamps at 31 rather than wrapping into green. */
    ASSERT_EQ((bright >> 10u) & 0x1Fu, 31u);
    /* Red is 4, so 4 * 4 = 16 scales without clamping. */
    ASSERT_EQ(bright & 0x1Fu, 16u);

    /* A colour-bank value (bit 15 clear) keeps bit 15 clear. */
    ASSERT_EQ(shade_rgb555(SAT_BGR555(31, 31, 31), SAT_FX16_ONE) & 0x8000u, 0u);
}

/* Maze walls are axis-aligned, so the four possible normals must produce four
 * distinguishable intensities or corners visually vanish. */
TEST(face_intensity_separates_all_four_normals) {
    const sat_fx16_t floor_i = SAT_FX16_ONE / 4;
    const sat_fx16_t px = face_intensity(SAT_FX16_ONE, 0, floor_i);
    const sat_fx16_t nx = face_intensity(-SAT_FX16_ONE, 0, floor_i);
    const sat_fx16_t pz = face_intensity(0, SAT_FX16_ONE, floor_i);
    const sat_fx16_t nz = face_intensity(0, -SAT_FX16_ONE, floor_i);

    ASSERT_TRUE(px != pz);
    ASSERT_TRUE(px > nx);
    ASSERT_TRUE(pz > nz);
    /* Faces turned away never go below the floor, so they stay readable. */
    ASSERT_EQ(nx, floor_i);
    ASSERT_EQ(nz, floor_i);
    /* And nothing exceeds full brightness. */
    ASSERT_TRUE(px <= SAT_FX16_ONE);
    ASSERT_TRUE(pz <= SAT_FX16_ONE);
}

TEST(face_intensity_clamps_floor_argument) {
    ASSERT_EQ(face_intensity(-SAT_FX16_ONE, 0, -SAT_FX16_ONE), 0);
    ASSERT_EQ(face_intensity(-SAT_FX16_ONE, 0, fx_from_int(5)), SAT_FX16_ONE);
}


/* The 2D light is flat: it cannot tell a floor from a ceiling, because both
 * have nx = nz = 0. The 3D form exists for exactly that case. */
TEST(face_intensity3_separates_up_from_down) {
    const sat_vec3_t up = { 0, SAT_FX16_ONE, 0 };
    const sat_vec3_t down = { 0, -SAT_FX16_ONE, 0 };
    const sat_fx16_t ambient = SAT_FX16_ONE / 4;
    const sat_fx16_t lit = face_intensity3(up, ambient);
    const sat_fx16_t dark = face_intensity3(down, ambient);
    ASSERT_TRUE(lit > dark);
    ASSERT_EQ(dark, ambient);
    ASSERT_TRUE(lit <= SAT_FX16_ONE);
    ASSERT_EQ(face_intensity(0, 0, ambient), face_intensity(0, 0, ambient));
}

TEST(face_intensity3_horizontal_matches_the_ground_light_direction) {
    /* Same horizontal bearing as kLightX/kLightZ, so a mesh face and a maze
     * wall pointing the same way are both on the lit side. */
    const sat_vec3_t towards = { kLight3X, 0, kLight3Z };
    const sat_vec3_t away = { -kLight3X, 0, -kLight3Z };
    ASSERT_TRUE(face_intensity3(towards, 0) > face_intensity3(away, 0));
    ASSERT_EQ(face_intensity3(away, 0), 0);
}

TEST(face_intensity3_clamps_its_floor) {
    const sat_vec3_t up = { 0, SAT_FX16_ONE, 0 };
    ASSERT_EQ(face_intensity3(up, -SAT_FX16_ONE), face_intensity3(up, 0));
    ASSERT_EQ(face_intensity3(up, fx_from_int(4)), SAT_FX16_ONE);
}


/* The scaled form is what sat_draw_mesh actually calls, so it has to agree
 * with the unit-normal form -- and be indifferent to how long the normal is,
 * since a cross product's length depends on the size of the face. */
TEST(face_intensity3_scaled_matches_the_unit_form_at_any_length) {
    const sat_vec3_t unit = { 0, SAT_FX16_ONE, 0 };
    const sat_fx16_t ambient = SAT_FX16_ONE / 4;
    const sat_fx16_t expected = face_intensity3(unit, ambient);

    for (int scale = 1; scale <= 4096; scale *= 8) {
        const sat_vec3_t scaled = { 0, SAT_FX16_ONE * scale, 0 };
        ASSERT_NEAR(face_intensity3_scaled(scaled, ambient), expected, 64);
    }
}

TEST(face_intensity3_scaled_handles_a_degenerate_normal) {
    const sat_vec3_t zero = { 0, 0, 0 };
    ASSERT_EQ(face_intensity3_scaled(zero, SAT_FX16_ONE / 2), SAT_FX16_ONE / 2);
}

int main() {
    wall_quad_corner_order();
    floor_quad_is_flat_and_centred();
    billboard_spreads_along_right_vector();
    project_axis_point_to_native_origin();
    project_respects_screen_axes();
    project_rejects_points_behind_camera();
    project_quad_rejects_straddling_quads();
    project_clamps_instead_of_wrapping();
    project_quad_rejects_null_arguments();
    sort_orders_far_to_near();
    sort_handles_degenerate_counts();
    ground_distance_saturates();
    rgb555_macro_matches_named_colors();
    shade_scales_channels();
    shade_preserves_rgb_code_bit_and_saturates();
    face_intensity_separates_all_four_normals();
    face_intensity_clamps_floor_argument();
    face_intensity3_separates_up_from_down();
    face_intensity3_horizontal_matches_the_ground_light_direction();
    face_intensity3_clamps_its_floor();
    face_intensity3_scaled_matches_the_unit_form_at_any_length();
    face_intensity3_scaled_handles_a_degenerate_normal();

    printf("PASS: test_render3d_logic.cpp (%d tests)\n", 22);
    return 0;
}
