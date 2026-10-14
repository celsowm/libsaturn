/* test_math3d_logic.cpp — host tests for the 3D math module */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/core.h"
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

TEST(mat4_products_match_reference_with_aliasing) {
    uint32_t seed = 0x12345678u;
    for (unsigned sample = 0; sample < 1000; ++sample) {
        sat_mat4_t a, b, expected{}, out;
        for (int i = 0; i < 16; ++i) {
            seed = seed * 1664525u + 1013904223u;
            a.m[i] = static_cast<int32_t>(seed % 524289u) - 262144;
            seed = seed * 1664525u + 1013904223u;
            b.m[i] = static_cast<int32_t>(seed % 524289u) - 262144;
        }
        // Both affine, only A affine, only B affine, neither affine.
        if (sample % 4 < 2) {
            a.m[12] = a.m[13] = a.m[14] = 0; a.m[15] = SAT_FX16_ONE;
        }
        if (sample % 4 == 0 || sample % 4 == 2) {
            b.m[12] = b.m[13] = b.m[14] = 0; b.m[15] = SAT_FX16_ONE;
        }
        // Independent full 4x4 reference, per-product arithmetic shift.
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                for (int k = 0; k < 4; ++k)
                    expected.m[row * 4 + col] += static_cast<int32_t>(
                        (static_cast<int64_t>(a.m[row * 4 + k]) * b.m[k * 4 + col]) >> 16);
        ASSERT_EQ(sat_mat4_multiply(&out, &a, &b), SAT_OK);
        sat_mat4_t alias_a = a, alias_b = b;
        ASSERT_EQ(sat_mat4_multiply(&alias_a, &alias_a, &b), SAT_OK);
        ASSERT_EQ(sat_mat4_multiply(&alias_b, &a, &alias_b), SAT_OK);
        for (int i = 0; i < 16; ++i) {
            ASSERT_EQ(out.m[i], expected.m[i]);
            ASSERT_EQ(alias_a.m[i], expected.m[i]);
            ASSERT_EQ(alias_b.m[i], expected.m[i]);
        }
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

TEST(core_helpers_isqrt_triangle_fx16_fourcc) {
    for (uint32_t v = 0u; v < 70000u; ++v) {
        const uint32_t r = sat_isqrt32(v);
        ASSERT_TRUE(r * r <= v && (r + 1u) * (r + 1u) > v);
    }
    ASSERT_EQ(sat_isqrt32(0xFFFFFFFFu), 65535u);
    ASSERT_EQ(sat_isqrt32(0x80000000u), 46340u);
    ASSERT_EQ(sat_triangle_wave(0u, 8u, 100), 0);
    ASSERT_EQ(sat_triangle_wave(4u, 8u, 100), 100);
    ASSERT_EQ(sat_triangle_wave(6u, 8u, 100), 50);
    ASSERT_EQ(sat_triangle_wave(12u, 8u, 100), 100);
    ASSERT_EQ(sat_triangle_wave(3u, 1u, 100), 0);
    ASSERT_EQ(SAT_FX16(3), 3 * 65536);
    ASSERT_EQ(SAT_FX16(-2), -2 * 65536);
    ASSERT_EQ(SAT_FX16(0.25), 16384);
    ASSERT_EQ(SAT_FOURCC('L', 'I', 'T', '1'), 0x4C495431u);
    ASSERT_EQ(SAT_CLAMP(9, 0, 5), 5);
    ASSERT_EQ(SAT_MIN(2, 3), 2);
}

/* The reference is the previous 64-bit per-bit scaling, including signed
 * rounding. Exercise narrow and wide cross products and zero degeneracy. */
TEST(vec3_cross_scaled_matches_wide_reference) {
    uint32_t random = 0x61c8591u;
    auto check = [](sat_vec3_t a, sat_vec3_t b) {
        int64_t x = (int64_t)a.y*b.z - (int64_t)a.z*b.y;
        int64_t y = (int64_t)a.z*b.x - (int64_t)a.x*b.z;
        int64_t z = (int64_t)a.x*b.y - (int64_t)a.y*b.x;
        auto magnitude = [](int64_t value) {return value < 0 ? -value : value;};
        int64_t maximum = magnitude(x);
        if(magnitude(y)>maximum)maximum=magnitude(y);
        if(magnitude(z)>maximum)maximum=magnitude(z);
        while(maximum > (1LL<<20)) {x>>=1;y>>=1;z>>=1;maximum>>=1;}
        const auto actual=vec3_cross_scaled(a,b);
        ASSERT_EQ(actual.x,x);ASSERT_EQ(actual.y,y);ASSERT_EQ(actual.z,z);
    };
    for(unsigned i=0;i<100000;++i) {
        sat_vec3_t a,b;
        int32_t* values[]={&a.x,&a.y,&a.z,&b.x,&b.y,&b.z};
        const unsigned bits=1+i%29;
        const uint32_t mask=(1u<<bits)-1;
        for(auto value:values) {
            random=random*1664525u+1013904223u;
            *value=(int32_t)(random&mask)-(int32_t)(1u<<(bits-1));
        }
        check(a,b);
    }
    for(unsigned bit=0;bit<30;++bit)for(int residue=-1;residue<=1;++residue) {
        int32_t value=(1<<bit)+residue;
        check({0,value,0},{0,0,1});
        check({0,-value,0},{0,0,1});
        check({1,value,-value},{value,-1,1});
    }
    check({0,0,0},{0,0,0});
    check({INT32_MAX,0,0},{0,INT32_MAX,0});
    check({INT32_MIN,0,0},{0,INT32_MIN,0});
}

int main() {
    vec3_cross_scaled_matches_wide_reference();
    fx_mul_and_div_roundtrip();
    fx_sqrt_exact();
    fx_sqrt_is_floor_root();
    sin_cos_cardinal_values();
    sin_45_degrees();
    mat4_identity_multiply();
    mat4_products_match_reference_with_aliasing();
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
    core_helpers_isqrt_triangle_fx16_fourcc();

    printf("PASS: test_math3d_logic.cpp (%d tests)\n", 23);
    return 0;
}
