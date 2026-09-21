/* test_render3d_logic.cpp — host tests for world-quad projection and shading */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/math3d.h"
#include "saturn/color.h"
#include "saturn/render3d.h"
#include "src/core/render3d_logic.hpp"
#include "src/core/mesh3d_logic.hpp"
#include "src/core/logic.hpp"

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

/* Regression: walking toward the SECOND FIXED platform (stage 1, Z=36)
 * carries the follow camera through the first pier's near-plane region.
 * A huge floor quad at Z=-18..18, Y=0 used to project at tiny positive
 * depth and then vanish completely as soon as one corner crossed behind.
 * The software clipper must retain only the visible portion, in bounded
 * triangles. No platform state or input changes here. */
TEST(clip_near_oversized_previous_pier) {
    sat_quad3_t pier;
    make_floor(&pier,0,0,0,fx_from_int(18));
    sat_quad3_t original=pier;
    const sat_vec3_t forward={0,-SAT_FX16_ONE*2/5,SAT_FX16_ONE*9/10};
    sat_quad3_t out[4];
    for(int camera_z=-12;camera_z<=-7;++camera_z) {
        const sat_vec3_t eye={fx_from_int(7),fx_from_int(29),
                              fx_from_int(camera_z)};
        uint8_t count=99u;
        ASSERT_EQ(clip_world_quad_near(
            &pier,&eye,&forward,fx_from_int(8),out,&count),SAT_OK);
        ASSERT_TRUE(count>=1u && count<=4u);
        for(uint8_t i=0u;i<count;++i) {
            for(uint8_t j=0u;j<4u;++j) {
                ASSERT_TRUE(world_view_depth(out[i].v[j],eye,forward)>=
                            fx_from_int(8)-128);
            }
        }
    }
    for(int i=0;i<4;++i) {
        ASSERT_EQ(pier.v[i].x,original.v[i].x);
        ASSERT_EQ(pier.v[i].y,original.v[i].y);
        ASSERT_EQ(pier.v[i].z,original.v[i].z);
    }
    /* Fully in front preserves the original quad (and texture UV order).
     * Fully behind produces zero quads, not an oversized billboard. */
    const sat_vec3_t eye={0,0,fx_from_int(100)};
    const sat_vec3_t along_neg_z={0,0,-SAT_FX16_ONE};
    sat_quad3_t wall;
    make_wall(&wall,fx_from_int(-10),fx_from_int(80),
                fx_from_int(10),fx_from_int(80),fx_from_int(12));
    uint8_t count=99u;
    ASSERT_EQ(clip_world_quad_near(
        &wall,&eye,&along_neg_z,fx_from_int(8),out,&count),SAT_OK);
    ASSERT_EQ(count,1u);
    ASSERT_EQ(out[0].v[0].z,wall.v[0].z);
    make_wall(&wall,fx_from_int(-10),fx_from_int(96),
                fx_from_int(10),fx_from_int(96),fx_from_int(12));
    ASSERT_EQ(clip_world_quad_near(
        &wall,&eye,&along_neg_z,fx_from_int(8),out,&count),SAT_OK);
    ASSERT_EQ(count,0u);
}
TEST(clip_near_generates_projectable_triangles) {
    const sat_vec3_t eye={0,0,fx_from_int(100)};
    const sat_vec3_t forward={0,0,-SAT_FX16_ONE};
    const sat_mat4_t vp=make_view_proj(100);
    sat_quad3_t crossing;
    crossing.v[0]={-fx_from_int(5),fx_from_int(5),fx_from_int(99)};
    crossing.v[1]={ fx_from_int(5),fx_from_int(5),fx_from_int(80)};
    crossing.v[2]={ fx_from_int(5),0,fx_from_int(80)};
    crossing.v[3]={-fx_from_int(5),0,fx_from_int(99)};
    sat_quad3_t parts[4];
    sat_quad2_t screen;
    uint8_t count=99u;
    ASSERT_EQ(clip_world_quad_near(
        &crossing,&eye,&forward,fx_from_int(8),parts,&count),SAT_OK);
    ASSERT_TRUE(count>=1u&&count<=4u);
    for(uint8_t i=0u;i<count;++i) {
        ASSERT_EQ(parts[i].v[2].z,parts[i].v[3].z);
        ASSERT_TRUE(project_quad(vp.m,&parts[i],kW,kH,&screen));
    }
    ASSERT_EQ(clip_world_quad_near(
        nullptr,&eye,&forward,fx_from_int(8),parts,&count),
        SAT_ERR_INVALID_ARG);
}

/* A quad straddling the view plane may project into +/-2047 pixel
 * coordinates. Native viewport clipping must constrain every submitted
 * solid triangle to the 320x224 target without losing the visible area.
 * The Skybridge HUD is submitted later to that same VDP1 command list. */
TEST(screen_clip_bounds_huge_pier_and_jump_views) {
    sat_quad2_t huge={};
    huge.x[0]=-2047;huge.y[0]=-2047;
    huge.x[1]=2047; huge.y[1]=-2047;
    huge.x[2]=2047; huge.y[2]=2047;
    huge.x[3]=-2047;huge.y[3]=2047;
    sat_quad2_t clipped[6];
    uint8_t count=99u;
    ASSERT_EQ(clip_quad_screen(&huge,320u,224u,clipped,&count),SAT_OK);
    ASSERT_TRUE(count>=1u && count<=6u);
    for(uint8_t i=0u;i<count;++i)for(uint8_t j=0u;j<4u;++j) {
        ASSERT_TRUE(clipped[i].x[j]>=-160 && clipped[i].x[j]<=159);
        ASSERT_TRUE(clipped[i].y[j]>=-112 && clipped[i].y[j]<=111);
    }
    /* Patterned overlays and ordinary sprites retain their original UV
     * corners only when completely inside the screen. */
    sat_quad2_t inside={};
    inside.x[0]=-30;inside.x[1]=30;inside.x[2]=30;inside.x[3]=-30;
    inside.y[0]=-20;inside.y[1]=-20;inside.y[2]=20;inside.y[3]=20;
    ASSERT_EQ(clip_quad_screen(&inside,320u,224u,clipped,&count),SAT_OK);
    ASSERT_EQ(count,1u);
    for(uint8_t j=0u;j<4u;++j) {
        ASSERT_EQ(clipped[0].x[j],inside.x[j]);
        ASSERT_EQ(clipped[0].y[j],inside.y[j]);
    }
    /* Entirely out-of-view: no VDP1 commands at all. */
    for(uint8_t j=0;j<4u;++j)inside.x[j]+=1000;
    ASSERT_EQ(clip_quad_screen(&inside,320u,224u,clipped,&count),SAT_OK);
    ASSERT_EQ(count,0u);
    /* Narrowly crossing the right edge on different jump heights is
     * supported without a giant raster or wraparound. */
    for(int height=-110;height<=110;height+=11) {
        inside.x[0]=145;inside.x[1]=170;
        inside.x[2]=170;inside.x[3]=145;
        inside.y[0]=static_cast<int16_t>(height-6);
        inside.y[1]=static_cast<int16_t>(height-6);
        inside.y[2]=static_cast<int16_t>(height+6);
        inside.y[3]=static_cast<int16_t>(height+6);
        ASSERT_EQ(clip_quad_screen(&inside,320u,224u,clipped,&count),SAT_OK);
        ASSERT_TRUE(count<=6u);
        for(uint8_t i=0u;i<count;++i)for(uint8_t j=0u;j<4u;++j) {
            ASSERT_TRUE(clipped[i].x[j]>=-160 && clipped[i].x[j]<=159);
            ASSERT_TRUE(clipped[i].y[j]>=-112 && clipped[i].y[j]<=111);
        }
    }
    ASSERT_EQ(clip_quad_screen(nullptr,320u,224u,clipped,&count),
              SAT_ERR_INVALID_ARG);
}
/* Reproduce the reported fixed C1 platform (index 1 at Z=36) with the
 * player taking steps from Z=30 to 33 and jumping up to ten world units.
 * The camera is 42 behind the player and 29 above, looking 50 forward
 * and 24 down. After near+viewport clipping, NO VDP1 polygon may use
 * out-of-screen coordinates, irrespective of a skybox or HUD draw order. */
TEST(skybridge_stage_one_step_and_jump_projection_stays_bounded) {
    const sat_vec3_t up={0,SAT_FX16_ONE,0};
    const sat_vec3_t previous_pier[4]={
        {-fx_from_int(18),0,-fx_from_int(18)},
        { fx_from_int(18),0,-fx_from_int(18)},
        { fx_from_int(18),0, fx_from_int(18)},
        {-fx_from_int(18),0, fx_from_int(18)}
    };
    const sat_vec3_t current_pier[4]={
        {-fx_from_int(13),0,fx_from_int(23)},
        { fx_from_int(13),0,fx_from_int(23)},
        { fx_from_int(13),0,fx_from_int(49)},
        {-fx_from_int(13),0,fx_from_int(49)}
    };
    for(int player_z=30;player_z<=33;++player_z) {
        for(int player_y=0;player_y<=10;player_y+=2) {
            const sat_vec3_t eye={
                fx_from_int(7),fx_from_int(29+player_y),
                fx_from_int(player_z-42)};
            const sat_vec3_t target={
                fx_from_int(7),fx_from_int(5+player_y),
                fx_from_int(player_z+8)};
            sat_vec3_t look={
                target.x-eye.x,target.y-eye.y,target.z-eye.z};
            sat_vec3_t forward;
            sat_vec3_normalize(&forward,&look);
            sat_mat4_t view,projection,vp;
            ASSERT_EQ(sat_mat4_look_at(&view,&eye,&target,&up),SAT_OK);
            ASSERT_EQ(sat_mat4_perspective(
                &projection,fx_from_int(55),
                sat_fx16_div(fx_from_int(320),fx_from_int(224)),
                fx_from_int(2),fx_from_int(250)),SAT_OK);
            ASSERT_EQ(sat_mat4_multiply(&vp,&projection,&view),SAT_OK);
            for(int deck=0;deck<2;++deck) {
                sat_quad3_t world,near_pieces[4];
                sat_quad2_t projected,screen_pieces[6];
                for(int k=0;k<4;++k)
                    world.v[k]=deck?current_pier[k]:previous_pier[k];
                uint8_t near_count=0u;
                ASSERT_EQ(clip_world_quad_near(
                    &world,&eye,&forward,fx_from_int(8),
                    near_pieces,&near_count),SAT_OK);
                ASSERT_TRUE(near_count<=4u);
                for(uint8_t i=0u;i<near_count;++i) {
                    ASSERT_TRUE(project_quad(
                        vp.m,&near_pieces[i],320,224,&projected));
                    uint8_t screen_count=0u;
                    ASSERT_EQ(clip_quad_screen(
                        &projected,320u,224u,screen_pieces,&screen_count),
                        SAT_OK);
                    ASSERT_TRUE(screen_count<=6u);
                    for(uint8_t tri=0u;tri<screen_count;++tri)
                        for(uint8_t j=0u;j<4u;++j) {
                            ASSERT_TRUE(screen_pieces[tri].x[j]>=-160);
                            ASSERT_TRUE(screen_pieces[tri].x[j]<=159);
                            ASSERT_TRUE(screen_pieces[tri].y[j]>=-112);
                            ASSERT_TRUE(screen_pieces[tri].y[j]<=111);
                        }
                }
            }
        }
    }
}
/* A three-vertex near-clipped polygon is ONE drawable triangle, not an
 * unchanged four-corner textured quad. Count==1 alone was incorrectly
 * treated as UV-safe by Skybridge, allowing the original giant inset
 * command to bypass near clipping. */
TEST(single_near_clipped_triangle_changes_original_texture_corners) {
    const sat_vec3_t eye={0,0,fx_from_int(100)};
    const sat_vec3_t forward={0,0,-SAT_FX16_ONE};
    sat_quad3_t quad={};
    quad.v[0]={-fx_from_int(8),0,fx_from_int(95)};
    quad.v[1]={ fx_from_int(8),0,fx_from_int(95)};
    quad.v[2]={ fx_from_int(8),0,fx_from_int(80)};
    quad.v[3]={-fx_from_int(8),0,fx_from_int(95)};
    sat_quad3_t clipped[4];
    uint8_t count=0u;
    ASSERT_EQ(clip_world_quad_near(
        &quad,&eye,&forward,fx_from_int(8),clipped,&count),SAT_OK);
    ASSERT_EQ(count,1u);
    bool unchanged=true;
    for(uint8_t i=0u;i<4u;++i) {
        if(clipped[0].v[i].x!=quad.v[i].x ||
           clipped[0].v[i].y!=quad.v[i].y ||
           clipped[0].v[i].z!=quad.v[i].z)unchanged=false;
    }
    ASSERT_FALSE(unchanged);
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

/* The stable insertion sort the heapsort replaced, kept as the reference. */
static void reference_sort16(uint16_t* idx, const uint32_t* keys, uint32_t n) {
    for (uint32_t i = 1; i < n; ++i) {
        const uint16_t value = idx[i];
        int64_t j = (int64_t)i - 1;
        while (j >= 0 && keys[idx[j]] < keys[value]) {
            idx[j + 1] = idx[j];
            --j;
        }
        idx[j + 1] = value;
    }
}

TEST(heap_sort_matches_stable_reference) {
    static uint32_t keys[600];
    static uint16_t got[600];
    static uint16_t want[600];
    static uint8_t got8[255];
    const uint32_t sizes[] = {0u, 1u, 2u, 3u, 17u, 255u, 600u};
    uint32_t seed = 12345u;
    for (uint32_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
        const uint32_t n = sizes[s];
        for (uint32_t i = 0; i < n; ++i) {
            seed = seed * 1103515245u + 12345u;
            keys[i] = (seed >> 16) % 23u; /* plenty of ties */
            got[i] = (uint16_t)i;
            want[i] = (uint16_t)i;
        }
        reference_sort16(want, keys, n);
        sort_indices16_desc(got, keys, n);
        for (uint32_t i = 0; i < n; ++i) {
            ASSERT_EQ(got[i], want[i]);
        }
        if (n <= 255u) {
            for (uint32_t i = 0; i < n; ++i) {
                got8[i] = (uint8_t)i;
            }
            sort_indices_desc(got8, keys, (uint16_t)n);
            for (uint32_t i = 0; i < n; ++i) {
                ASSERT_EQ(got8[i], want[i]);
            }
        }
    }
}

/* project_axis must equal the clamped division it replaced, including right
 * at the clamp boundary and for divisors past 32 bits. */
TEST(project_axis_matches_clamped_division) {
    const int64_t divisors[] = {1, 2, 7, 1000, 65536, 0x7FFFFFFF,
                                (int64_t)0x7FFFFFFF + 1, (int64_t)1 << 40};
    const int64_t quotients[] = {-5000, -2049, -2048, -2047, -1, 0, 1, 2047, 2048, 2049, 5000};
    for (int64_t w2 : divisors) {
        for (int64_t q : quotients) {
            for (int64_t d = -1; d <= 1; ++d) {
                const int64_t num = q * w2 + d;
                ASSERT_EQ(project_axis(num, w2), clamp_coord64(num / w2));
            }
        }
        const int64_t huge = (int64_t)1 << 60;
        ASSERT_EQ(project_axis(huge, w2), clamp_coord64(huge / w2));
        ASSERT_EQ(project_axis(-huge, w2), clamp_coord64(-huge / w2));
    }
}

TEST(project_quad_reuses_repeated_corners) {
    const sat_mat4_t vp = make_view_proj(100);
    sat_quad3_t tri;
    make_floor(&tri, fx_from_int(3), fx_from_int(-2), fx_from_int(-5), fx_from_int(7));
    tri.v[3] = tri.v[2];
    sat_quad2_t out;
    ASSERT_TRUE(project_quad(vp.m, &tri, kW, kH, &out));
    for (int i = 0; i < 4; ++i) {
        int16_t x = 0;
        int16_t y = 0;
        ASSERT_TRUE(project_native(vp.m, tri.v[i].x, tri.v[i].y, tri.v[i].z, kW, kH, &x, &y));
        ASSERT_EQ(out.x[i], x);
        ASSERT_EQ(out.y[i], y);
    }
    /* A fan repeats its hub as the first and last corner. */
    sat_quad3_t fan = tri;
    fan.v[3] = fan.v[0];
    ASSERT_TRUE(project_quad(vp.m, &fan, kW, kH, &out));
    ASSERT_EQ(out.x[3], out.x[0]);
    ASSERT_EQ(out.y[3], out.y[0]);
}

static sat_mat4_t view_proj_looking_at_origin(sat_vec3_t eye) {
    sat_mat4_t view;
    sat_mat4_t proj;
    sat_mat4_t vp;
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

TEST(project_vertex_matches_project_native) {
    const sat_mat4_t vp = make_view_proj(100);
    const sat_vec3_t points[] = {
        {0, 0, 0},
        {fx_from_int(10), fx_from_int(-7), fx_from_int(3)},
        {fx_from_int(-40), fx_from_int(25), fx_from_int(-60)},
        {fx_from_int(900), fx_from_int(900), fx_from_int(90)},
    };
    for (const sat_vec3_t& p : points) {
        sat_projected_vertex_t pv = {};
        project_vertex(vp.m, p, kW, kH, &pv);
        int16_t x = 0;
        int16_t y = 0;
        const bool in_front = project_native(vp.m, p.x, p.y, p.z, kW, kH, &x, &y);
        ASSERT_TRUE(in_front == (pv.w > 0));
        if (in_front) {
            ASSERT_EQ(pv.x, x);
            ASSERT_EQ(pv.y, y);
        }
    }
    sat_projected_vertex_t behind = {};
    project_vertex(vp.m, sat_vec3_t{0, 0, fx_from_int(200)}, kW, kH, &behind);
    ASSERT_TRUE(behind.w <= 0);
}

/* Screen-space culling (signed projected area) must agree with the world
 * test (outward normal towards the eye) for every face of a closed box. */
TEST(screen_area_culling_matches_world_culling) {
    namespace mesh3d = saturn::core::mesh3d;
    sat_vec3_t verts[8];
    uint16_t indices[24];
    sat_mesh_t mesh;
    ASSERT_EQ(mesh3d::init(&mesh, verts, 8, indices, 6), SAT_OK);
    ASSERT_EQ(mesh3d::build_box(&mesh, sat_vec3_t{0, 0, 0},
                                fx_from_int(4), fx_from_int(5), fx_from_int(3)), SAT_OK);
    const sat_vec3_t eyes[] = {
        {0, 0, fx_from_int(30)},
        {fx_from_int(30), fx_from_int(10), fx_from_int(2)},
        {fx_from_int(-20), fx_from_int(25), fx_from_int(-20)},
        {fx_from_int(6), fx_from_int(-30), fx_from_int(9)},
    };
    for (const sat_vec3_t& eye : eyes) {
        const sat_mat4_t vp = view_proj_looking_at_origin(eye);
        int front = 0;
        for (uint16_t f = 0; f < mesh.face_count; ++f) {
            sat_quad3_t quad;
            ASSERT_EQ(mesh3d::face_quad(&mesh, f, &quad), SAT_OK);
            sat_quad2_t projected;
            for (int i = 0; i < 4; ++i) {
                sat_projected_vertex_t pv = {};
                project_vertex(vp.m, quad.v[i], kW, kH, &pv);
                ASSERT_TRUE(pv.w > 0);
                projected.x[i] = pv.x;
                projected.y[i] = pv.y;
            }
            const bool screen_front = quad2_area2(projected) > 0;
            ASSERT_TRUE(screen_front == mesh3d::quad_visible(quad, eye));
            front += screen_front ? 1 : 0;
        }
        ASSERT_TRUE(front >= 1 && front <= 3);
    }
}

/* Gouraud table entries, light-to-entry mapping and command encoding. */
TEST(gouraud_table_words_and_light) {
    ASSERT_EQ(SAT_GOURAUD_NEUTRAL, 0x4210);
    ASSERT_EQ(sat_gouraud_grey(0), SAT_GOURAUD_NEUTRAL);
    ASSERT_EQ(sat_gouraud_rgb(-16, 0, 15), (uint16_t)((31 << 10) | (16 << 5) | 0));
    ASSERT_EQ(sat_gouraud_rgb(-99, 99, 3), (uint16_t)((19 << 10) | (31 << 5) | 0));
    ASSERT_EQ(gouraud_grey_word(5), sat_gouraud_grey(5));
    ASSERT_EQ(gouraud_grey_word(-40), 0x0000);
    ASSERT_EQ(gouraud_from_intensity(SAT_FX16_ONE), SAT_GOURAUD_NEUTRAL);
    ASSERT_EQ(gouraud_from_intensity(0), 0x0000);
    ASSERT_EQ(gouraud_from_intensity(2 * SAT_FX16_ONE), 0x7FFF);
    ASSERT_EQ(gouraud_from_intensity(SAT_FX16_ONE / 2), sat_gouraud_grey(-8));
    const sat_vec3_t up = {0, SAT_FX16_ONE, 0};
    const sat_vec3_t down = {0, -SAT_FX16_ONE, 0};
    ASSERT_EQ(gouraud_lambert_word(up, up, SAT_FX16_ONE / 4), SAT_GOURAUD_NEUTRAL);
    ASSERT_EQ(gouraud_lambert_word(down, up, SAT_FX16_ONE / 4), sat_gouraud_grey(-12));
    ASSERT_EQ(saturn::core::compose_gouraud_pmod(saturn::core::compose_polygon_pmod(0)) & 0x7u, 4u);
    ASSERT_EQ(saturn::core::compose_gouraud_pmod(saturn::core::compose_polygon_pmod(0)) & 0xFFF8u,
              saturn::core::compose_polygon_pmod(0));
    ASSERT_EQ(saturn::core::gouraud_table_grda(64u * 1024u, 3), (uint16_t)(8192 + 3));
}

/* Smooth vertex normals of a closed box point out of it, unit length. */
TEST(vertex_normals_point_out_of_a_box) {
    namespace mesh3d = saturn::core::mesh3d;
    sat_vec3_t verts[8];
    uint16_t indices[24];
    sat_mesh_t mesh;
    ASSERT_EQ(mesh3d::init(&mesh, verts, 8, indices, 6), SAT_OK);
    const sat_vec3_t center = {fx_from_int(2), fx_from_int(-1), fx_from_int(3)};
    ASSERT_EQ(mesh3d::build_box(&mesh, center, fx_from_int(4), fx_from_int(5), fx_from_int(3)), SAT_OK);
    sat_vec3_t normals[8];
    ASSERT_EQ(mesh3d::vertex_normals(&mesh, normals, 8), SAT_OK);
    for (int v = 0; v < 8; ++v) {
        const sat_vec3_t rel = {verts[v].x - center.x, verts[v].y - center.y, verts[v].z - center.z};
        ASSERT_TRUE(saturn::core::math3d::vec3_dot_raw(normals[v], rel) > 0);
        ASSERT_NEAR(saturn::core::math3d::fx_len3(normals[v].x, normals[v].y, normals[v].z),
                    SAT_FX16_ONE, 64);
    }
    ASSERT_EQ(mesh3d::vertex_normals(&mesh, normals, 7), SAT_ERR_CAPACITY);
}

/* The diagonal cross product is the shoelace sum, for any corners in range,
 * and the cache-entry form agrees with the quad form. */
TEST(diagonal_area_equals_shoelace) {
    uint32_t seed = 777u;
    for (int n = 0; n < 5000; ++n) {
        sat_quad2_t q;
        sat_projected_vertex_t screen[4];
        const uint16_t idx[4] = {0, 1, 2, 3};
        for (int i = 0; i < 4; ++i) {
            seed = seed * 1103515245u + 12345u;
            q.x[i] = (int16_t)((int32_t)((seed >> 8) % 4095u) - 2047);
            seed = seed * 1103515245u + 12345u;
            q.y[i] = (int16_t)((int32_t)((seed >> 8) % 4095u) - 2047);
        }
        if (n % 3 == 0) {
            q.x[3] = q.x[2]; /* degenerate triangle corner */
            q.y[3] = q.y[2];
        }
        int32_t shoelace = 0;
        for (int i = 0; i < 4; ++i) {
            const int j = (i + 1) & 3;
            shoelace += (int32_t)q.x[i] * q.y[j] - (int32_t)q.x[j] * q.y[i];
            screen[i].x = q.x[i];
            screen[i].y = q.y[i];
            screen[i].w = SAT_FX16_ONE;
        }
        ASSERT_EQ(quad2_area2(q), shoelace);
        ASSERT_EQ(projected_area2(screen, idx), shoelace);
    }
}

/* Bucketed painter's order: every live face exactly once, farther buckets
 * first, ascending face index inside a bucket, skipped faces left out. */
TEST(bucket_paint_order_is_complete_and_farthest_first) {
    static uint32_t keys[700];
    static uint32_t work[700];
    static uint32_t work8[255];
    static uint16_t out[700];
    static uint8_t out8[255];
    static uint8_t seen[700];
    const uint32_t sizes[] = {0u, 1u, 2u, 5u, 255u, 700u};
    uint32_t seed = 4242u;
    for (uint32_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
        const uint32_t n = sizes[s];
        uint32_t expect_live = 0u;
        uint32_t lo = 0xFFFFFFFFu;
        uint32_t hi = 0u;
        for (uint32_t i = 0; i < n; ++i) {
            seed = seed * 1103515245u + 12345u;
            keys[i] = ((seed >> 4) % 7u == 0u) ? kPaintSkip : ((seed >> 3) % 5000000u);
            work[i] = keys[i];
            if (keys[i] != kPaintSkip) {
                ++expect_live;
                lo = keys[i] < lo ? keys[i] : lo;
                hi = keys[i] > hi ? keys[i] : hi;
            }
        }
        const uint32_t live = paint_order_buckets(work, n, out);
        ASSERT_EQ(live, expect_live);
        uint32_t shift = 0u;
        while (live > 0u && ((hi - lo) >> shift) >= kPaintBuckets) {
            ++shift;
        }
        for (uint32_t i = 0; i < n; ++i) {
            seen[i] = 0u;
        }
        for (uint32_t i = 0; i < live; ++i) {
            ASSERT_TRUE(out[i] < n);
            ASSERT_TRUE(keys[out[i]] != kPaintSkip);
            ASSERT_EQ(seen[out[i]], 0u);
            seen[out[i]] = 1u;
            if (i + 1u < live) {
                const uint32_t b0 = (keys[out[i]] - lo) >> shift;
                const uint32_t b1 = (keys[out[i + 1u]] - lo) >> shift;
                ASSERT_TRUE(b0 > b1 || (b0 == b1 && out[i] < out[i + 1u]));
            }
        }
        if (n <= 255u) {
            for (uint32_t i = 0; i < n; ++i) {
                work8[i] = keys[i];
            }
            ASSERT_EQ(paint_order_buckets(work8, n, out8), live);
            for (uint32_t i = 0; i < live; ++i) {
                ASSERT_EQ(out8[i], out[i]);
            }
        }
    }
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


/* The scaled form is what the native mesh path actually calls, so it has to agree
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
    clip_near_oversized_previous_pier();
    clip_near_generates_projectable_triangles();
    screen_clip_bounds_huge_pier_and_jump_views();
    skybridge_stage_one_step_and_jump_projection_stays_bounded();
    single_near_clipped_triangle_changes_original_texture_corners();
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

    heap_sort_matches_stable_reference();
    project_axis_matches_clamped_division();
    project_quad_reuses_repeated_corners();
    project_vertex_matches_project_native();
    screen_area_culling_matches_world_culling();
    bucket_paint_order_is_complete_and_farthest_first();
    diagonal_area_equals_shoelace();
    gouraud_table_words_and_light();
    vertex_normals_point_out_of_a_box();
    printf("PASS: test_render3d_logic.cpp (%d tests)\n", 36);
    return 0;
}
