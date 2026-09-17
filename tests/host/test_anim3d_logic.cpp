/* test_anim3d_logic.cpp -- host tests for baked-animation assets. */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "saturn/anim3d.h"
#include "src/core/anim3d_logic.hpp"
#include "src/core/render3d_logic.hpp"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

namespace {

/* Two-vertex model: verts (0,0,0) and (1,1,1) in fixed point. */
sat_vec3_t kVerts[2] = {{0, 0, 0}, {65536, 65536, 65536}};
uint16_t kIndices[4] = {0, 1, 1, 0};
uint16_t kFaceTex[1] = {SAT_MESH_TEXTURE_NONE};
uint8_t kPixels[8] = {};
uint16_t kPalette[256] = {};
sat_model_texture_asset_t kTex[1] = {{kPixels, 8, 1, 0, 0, 8u}};
sat_model_asset_t kModel = {
    kVerts, 2, kIndices, 1, kFaceTex, kTex, 1, kPalette, 1, 1, 0, nullptr, 0, nullptr,
};

/* Three frames for two verts: frame f holds (f, f+100, f+200) raw q15. */
int16_t kPos[3 * 2 * 3] = {
    0, 100, 200, 1000, 1100, 1200,
    3000, 3100, 3200, 4000, 4100, 4200,
    6000, 6100, 6200, 7000, 7100, 7200,
};

sat_model_animation_asset_t kLoopClip = {
    kPos, 3, 2, 30, 1,
    SAT_ANIM_FLAG_LOOP, 0,
    {0, 0, 0, 65536, 65536, 65536}, /* bias 0, scale 1.0 */
    nullptr,
    nullptr,
};

sat_model_animation_asset_t kOnceClip = {
    kPos, 3, 2, 30, 1,
    0, 0,
    {0, 0, 0, 65536, 65536, 65536},
    nullptr,
    nullptr,
};

sat_model_animation_asset_t kClips[2];

sat_animated_model_asset_t make_asset(bool loop_first) {
    kClips[0] = loop_first ? kLoopClip : kOnceClip;
    kClips[1] = kOnceClip;
    sat_animated_model_asset_t a = {};
    a.model = &kModel;
    a.animations = kClips;
    a.animation_count = 2;
    return a;
}

}  // namespace

/* Stubs for the draw/upload paths (mesh3d_api/model3d_api references). */
extern "C" sat_result_t sat_draw_world_polygon(
    const sat_mat4_t*, const sat_quad3_t*, uint16_t) {
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_world_sprite(
    const sat_mat4_t*, const sat_quad3_t*,
    const sat_vdp1_texture_t*, uint16_t, uint16_t) {
    return SAT_OK;
}

extern "C" sat_result_t sat_project_vertices(
    const sat_mat4_t*, const sat_vec3_t*, uint16_t, sat_projected_vertex_t*) {
    return SAT_ERR_UNSUPPORTED;
}

extern "C" sat_result_t sat_draw_quad2_polygon(const sat_quad2_t*, uint16_t) {
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_quad2_sprite(
    const sat_quad2_t*, const sat_vdp1_texture_t*, uint16_t, uint16_t) {
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_quad2_polygon_gouraud(
    const sat_quad2_t*, uint16_t, const uint16_t*) {
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_world_polygon_gouraud(
    const sat_mat4_t*, const sat_quad3_t*, uint16_t, const uint16_t*) {
    return SAT_OK;
}

extern "C" sat_result_t sat_palette_upload_indexed8(const uint16_t*, uint16_t) {
    return SAT_OK;
}

extern "C" sat_result_t sat_tex_upload_indexed8_pixels(
    sat_vdp1_texture_t* out, const uint8_t*, uint16_t w, uint16_t h, uint16_t pal) {
    out->srca = 1;
    out->width = w;
    out->height = h;
    out->palette = pal;
    out->valid = 1;
    out->reserved = 0;
    return SAT_OK;
}

static void valid_descriptor_passes() {
    sat_animated_model_asset_t a = make_asset(true);
    ASSERT_EQ(sat_anim_validate(&a), SAT_OK);
    ASSERT_EQ(sat_anim_clip_validate(&a, 0), SAT_OK);
    ASSERT_EQ(sat_anim_clip_validate(&a, 1), SAT_OK);
}

static void malformed_descriptors_rejected() {
    sat_animated_model_asset_t a = make_asset(true);
    ASSERT_EQ(sat_anim_validate(nullptr), SAT_ERR_INVALID_ARG);
    sat_animated_model_asset_t bad = a;
    bad.model = nullptr;
    ASSERT_EQ(sat_anim_validate(&bad), SAT_ERR_INVALID_ARG);
    bad = a;
    bad.animation_count = 0;
    ASSERT_EQ(sat_anim_validate(&bad), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_anim_clip_validate(&a, 7), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_anim_clip_validate(nullptr, 0), SAT_ERR_INVALID_ARG);
    /* Zero rate denominator. */
    sat_model_animation_asset_t zero_rate = kLoopClip;
    zero_rate.sample_rate_den = 0;
    sat_model_animation_asset_t clips[1] = {zero_rate};
    sat_animated_model_asset_t c = {&kModel, clips, 1, 0};
    ASSERT_EQ(sat_anim_validate(&c), SAT_ERR_INVALID_ARG);
    /* Vertex-count mismatch with the static model. */
    sat_model_animation_asset_t bad_v = kLoopClip;
    bad_v.vertex_count = 5;
    sat_model_animation_asset_t clips2[1] = {bad_v};
    sat_animated_model_asset_t d = {&kModel, clips2, 1, 0};
    ASSERT_EQ(sat_anim_validate(&d), SAT_ERR_INVALID_ARG);
    /* Null positions. */
    sat_model_animation_asset_t null_p = kLoopClip;
    null_p.positions = nullptr;
    sat_model_animation_asset_t clips3[1] = {null_p};
    sat_animated_model_asset_t e = {&kModel, clips3, 1, 0};
    ASSERT_EQ(sat_anim_validate(&e), SAT_ERR_INVALID_ARG);
}

static void clip_duration_matches_rate() {
    sat_animated_model_asset_t a = make_asset(true);
    /* 3 frames at 30 fps = 0.1 s = 6553.6 -> 6553 in 16.16. */
    ASSERT_EQ(sat_anim_clip_duration(&a, 0), (3 * 65536) / 30);
    ASSERT_EQ(sat_anim_clip_duration(&a, 9), 0);
    ASSERT_EQ(sat_anim_clip_duration(nullptr, 0), 0);
}

static void init_set_reset() {
    sat_animated_model_asset_t a = make_asset(true);
    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 1), SAT_OK);
    ASSERT_EQ(st.clip, 1u);
    ASSERT_EQ(st.frame, 0u);
    ASSERT_EQ(st.time, 0);
    ASSERT_EQ(sat_anim_state_init(&st, &a, 9), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_anim_state_init(nullptr, &a, 0), SAT_ERR_INVALID_ARG);
    st.time = 1234;
    st.frame = 2;
    ASSERT_EQ(sat_anim_reset(&st), SAT_OK);
    ASSERT_EQ(st.time, 0);
    ASSERT_EQ(st.frame, 0u);
    ASSERT_EQ(st.clip, 1u);
    ASSERT_EQ(sat_anim_reset(nullptr), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_anim_set_clip(&st, &a, 0), SAT_OK);
    ASSERT_EQ(st.clip, 0u);
    ASSERT_EQ(st.time, 0);
}

static void pause_blocks_advance() {
    sat_animated_model_asset_t a = make_asset(true);
    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 0), SAT_OK);
    sat_anim_set_paused(&st, 1);
    ASSERT_TRUE(sat_anim_is_paused(&st) != 0);
    ASSERT_EQ(sat_anim_advance(&st, &a, 65536), SAT_OK);
    ASSERT_EQ(st.time, 0);
    ASSERT_EQ(st.frame, 0u);
    sat_anim_set_paused(&st, 0);
    ASSERT_TRUE(sat_anim_is_paused(&st) == 0);
    ASSERT_EQ(sat_anim_advance(&st, &a, 65536), SAT_OK);
    ASSERT_TRUE(st.time != 0);
    sat_anim_set_paused(nullptr, 1); /* null-safe no-op */
    ASSERT_EQ(sat_anim_is_paused(nullptr), 0);
}

static void ntsc_advance_steps_two_vblanks_per_frame() {
    sat_animated_model_asset_t a = make_asset(true);
    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 0), SAT_OK);
    /* One 30 Hz frame is 65536/30 = 2184.53 in 16.16; 2185 steps past it. */
    ASSERT_EQ(sat_anim_advance(&st, &a, 2185), SAT_OK);
    ASSERT_EQ(st.frame, 1u);
    ASSERT_EQ(sat_anim_reset(&st), SAT_OK);
    /* Truncated NTSC ticks accumulate honestly: 2x1092 = 2184 is still
     * short of a full frame, the third tick completes it. */
    const sat_fx16_t vblank = 65536 / 60;
    ASSERT_EQ(sat_anim_advance(&st, &a, vblank), SAT_OK);
    ASSERT_EQ(st.frame, 0u);
    ASSERT_EQ(sat_anim_advance(&st, &a, vblank), SAT_OK);
    ASSERT_EQ(st.frame, 0u);
    ASSERT_EQ(sat_anim_advance(&st, &a, vblank), SAT_OK);
    ASSERT_EQ(st.frame, 1u);
    /* A full duration from zero wraps the loop back to frame 0. */
    ASSERT_EQ(sat_anim_reset(&st), SAT_OK);
    ASSERT_EQ(sat_anim_advance(&st, &a, sat_anim_clip_duration(&a, 0)), SAT_OK);
    ASSERT_EQ(st.time, 0);
    ASSERT_EQ(st.frame, 0u);
}

static void pal_fractional_accumulator_wraps_cleanly() {
    sat_animated_model_asset_t a = make_asset(true);
    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 0), SAT_OK);
    const sat_fx16_t vblank = 65536 / 50; /* truncated PAL tick */
    const sat_fx16_t dur = sat_anim_clip_duration(&a, 0);
    for (int i = 0; i < 50; ++i) {
        ASSERT_EQ(sat_anim_advance(&st, &a, vblank), SAT_OK);
    }
    /* ~1 s elapsed on a 0.1 s loop: time must have wrapped, frame valid. */
    ASSERT_TRUE(st.time >= 0 && st.time < dur);
    ASSERT_TRUE(st.frame < 3u);
    /* Reference: integer frame walk of the same ticks. */
    int64_t t = 0;
    for (int i = 0; i < 50; ++i) {
        t += vblank;
        if (t >= dur) {
            t %= dur;
        }
    }
    int64_t want = (t * 30) / (1 * 65536);
    if (want >= 3) {
        want %= 3;
    }
    ASSERT_EQ(st.frame, (uint16_t)want);
}

static void loop_wraps_and_once_holds() {
    sat_animated_model_asset_t a = make_asset(true);
    sat_anim_state_t loop = {};
    ASSERT_EQ(sat_anim_state_init(&loop, &a, 0), SAT_OK);
    ASSERT_EQ(sat_anim_advance(&loop, &a, sat_anim_clip_duration(&a, 0) * 2 + 100), SAT_OK);
    ASSERT_TRUE(loop.time < sat_anim_clip_duration(&a, 0));
    ASSERT_TRUE(loop.frame < 3u);
    sat_anim_state_t once = {};
    ASSERT_EQ(sat_anim_state_init(&once, &a, 1), SAT_OK);
    ASSERT_EQ(sat_anim_advance(&once, &a, 65536 * 10), SAT_OK);
    ASSERT_EQ(once.frame, 2u); /* held on the final frame */
    ASSERT_EQ(sat_anim_advance(nullptr, &a, 100), SAT_ERR_INVALID_ARG);
}

static int32_t decode_expect(int32_t bias, int32_t scale, int16_t q) {
    return bias + (int32_t)(((int64_t)scale * (int64_t)q) / 32767);
}

static void decode_matches_quantization_contract() {
    sat_animated_model_asset_t a = make_asset(true);
    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 0), SAT_OK);
    sat_vec3_t out[2] = {};
    ASSERT_EQ(sat_anim_decode(&a, &st, out, 2), SAT_OK);
    /* Frame 0, vert 0: q = (0, 100, 200), bias 0, scale 1.0. */
    ASSERT_EQ(out[0].x, decode_expect(0, 65536, 0));
    ASSERT_EQ(out[0].y, decode_expect(0, 65536, 100));
    ASSERT_EQ(out[0].z, decode_expect(0, 65536, 200));
    ASSERT_EQ(out[1].x, decode_expect(0, 65536, 1000));
    /* One NTSC pair of ceiling ticks crosses one 30 Hz frame. */
    ASSERT_EQ(sat_anim_advance(&st, &a, 65536 / 60 + 1), SAT_OK);
    ASSERT_EQ(sat_anim_advance(&st, &a, 65536 / 60 + 1), SAT_OK);
    ASSERT_EQ(st.frame, 1u);
    ASSERT_EQ(sat_anim_decode(&a, &st, out, 2), SAT_OK);
    ASSERT_EQ(out[0].x, decode_expect(0, 65536, 3000));
    ASSERT_EQ(out[1].z, decode_expect(0, 65536, 4200));
}

static void decode_extremes_and_bias() {
    /* scale 0 axis always decodes to bias; q extremes hit bias +/- scale. */
    static int16_t pos[3] = {32767, -32767, 1234};
    static sat_model_animation_asset_t clip = {
        pos, 1, 1, 30, 1, SAT_ANIM_FLAG_LOOP, 0,
        {100, -50, 7, 1000, 2000, 0},
        nullptr,
        nullptr,
    };
    static sat_vec3_t mverts[1] = {{0, 0, 0}};
    static uint16_t midx[4] = {0, 0, 0, 0};
    static sat_model_asset_t m1 = {
        mverts, 1, midx, 1, kFaceTex, kTex, 1, kPalette, 1, 1, 0, nullptr, 0, nullptr,
    };
    static sat_model_animation_asset_t clips[1];
    clips[0] = clip;
    sat_animated_model_asset_t a = {&m1, clips, 1, 0};
    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 0), SAT_OK);
    sat_vec3_t out[1] = {{-1, -1, -1}};
    ASSERT_EQ(sat_anim_decode(&a, &st, out, 1), SAT_OK);
    ASSERT_EQ(out[0].x, 100 + 1000);   /* bias + scale */
    ASSERT_EQ(out[0].y, -50 - 2000);   /* bias - scale */
    ASSERT_EQ(out[0].z, 7);            /* scale 0 -> bias */
}

/* The divide-free decoder must reproduce the contract exactly: every q
 * extreme, scales of both signs up to the full fx16 range, and the largest
 * rest * q the reciprocal multiply has to cover. */
static void divide_free_decode_is_exact() {
    ASSERT_EQ(saturn::core::anim3d::div32767_small(32766 * 32767), 32766);
    ASSERT_EQ(saturn::core::anim3d::div32767_small(-(32766 * 32767)), -32766);
    ASSERT_EQ(saturn::core::anim3d::div32767_small(32766 * 32767 - 1), 32765);
    for (int32_t n = 0; n < 2000000; n += 7) {
        ASSERT_EQ(saturn::core::anim3d::div32767_small(n), n / 32767);
        ASSERT_EQ(saturn::core::anim3d::div32767_small(-n), -n / 32767);
    }
    const int32_t scales[] = {0, 1, -1, 32766, 32767, 32768, -32767, 65536, -65536,
                              1234567, -7654321, 0x7FFFFFFF, -0x7FFFFFFF};
    uint32_t seed = 99u;
    for (int32_t scale : scales) {
        const auto d = saturn::core::anim3d::make_axis_decoder(17, scale);
        for (int32_t q = -32767; q <= 32767; q += (q > -32700 && q < 32700) ? 97 : 1) {
            const int32_t expect = 17 + (int32_t)(((int64_t)scale * (int64_t)q) / 32767);
            ASSERT_EQ(saturn::core::anim3d::decode_axis(d, (int16_t)q), expect);
        }
    }
    for (int i = 0; i < 20000; ++i) {
        seed = seed * 1103515245u + 12345u;
        const int32_t scale = (int32_t)(seed ^ (seed << 7)) / 2;
        seed = seed * 1103515245u + 12345u;
        const int16_t q = (int16_t)(((seed >> 8) % 65535u) - 32767);
        const auto d = saturn::core::anim3d::make_axis_decoder(0, scale);
        const int32_t expect = (int32_t)(((int64_t)scale * (int64_t)q) / 32767);
        ASSERT_EQ(saturn::core::anim3d::decode_axis(d, q), expect);
    }
}

/* Baked shades: one palette lookup per face per frame, bounded at load. */
static void face_colors_follow_baked_shades() {
    static uint16_t shade_pal[3] = {0x8000u, 0x801Fu, 0xFC00u};
    static uint8_t shades[3] = {1, 2, 1}; /* 3 frames x 1 face */
    sat_model_asset_t model = kModel;
    model.shade_palette_rgb555 = shade_pal;
    model.shade_palette_count = 3;
    sat_model_animation_asset_t clips[1] = {kLoopClip};
    clips[0].face_shades = shades;
    sat_animated_model_asset_t a = {&model, clips, 1, 0};
    ASSERT_EQ(sat_anim_validate(&a), SAT_OK);

    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 0), SAT_OK);
    uint16_t out[1] = {0};
    ASSERT_EQ(sat_anim_face_colors(&a, &st, out, 1), SAT_OK);
    ASSERT_EQ(out[0], 0x801Fu);
    st.frame = 1;
    ASSERT_EQ(sat_anim_face_colors(&a, &st, out, 1), SAT_OK);
    ASSERT_EQ(out[0], 0xFC00u);
    ASSERT_EQ(sat_anim_face_colors(&a, &st, out, 0), SAT_ERR_CAPACITY);
    ASSERT_EQ(sat_anim_face_colors(&a, &st, nullptr, 1), SAT_ERR_INVALID_ARG);

    shades[2] = 7; /* past the palette */
    ASSERT_EQ(sat_anim_validate(&a), SAT_ERR_INVALID_ARG);
    shades[2] = 1;
    model.shade_palette_count = 0;
    ASSERT_EQ(sat_anim_validate(&a), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_anim_face_colors(&a, &st, out, 1), SAT_ERR_UNSUPPORTED);
    model.shade_palette_count = 3;
    clips[0].face_shades = nullptr;
    ASSERT_EQ(sat_anim_face_colors(&a, &st, out, 1), SAT_ERR_UNSUPPORTED);
}

/* Baked Gouraud: one white table entry per vertex per frame. */
static void vertex_gouraud_follows_baked_levels() {
    static uint8_t levels[3 * 2] = {16, 31, 0, 20, 5, 16}; /* 3 frames x 2 verts */
    sat_model_animation_asset_t clips[1] = {kLoopClip};
    clips[0].vertex_gouraud = levels;
    sat_animated_model_asset_t a = {&kModel, clips, 1, 0};
    ASSERT_EQ(sat_anim_validate(&a), SAT_OK);

    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 0), SAT_OK);
    uint16_t out[2] = {0, 0};
    ASSERT_EQ(sat_anim_vertex_gouraud(&a, &st, out, 2), SAT_OK);
    ASSERT_EQ(out[0], 0x4210u);
    ASSERT_EQ(out[1], 0x7FFFu);
    st.frame = 2;
    ASSERT_EQ(sat_anim_vertex_gouraud(&a, &st, out, 2), SAT_OK);
    ASSERT_EQ(out[0], 0x14A5u);
    ASSERT_EQ(out[1], 0x4210u);
    ASSERT_EQ(sat_anim_vertex_gouraud(&a, &st, out, 1), SAT_ERR_CAPACITY);

    levels[3] = 32; /* not a 5-bit level */
    ASSERT_EQ(sat_anim_validate(&a), SAT_ERR_INVALID_ARG);
    levels[3] = 20;
    clips[0].vertex_gouraud = nullptr;
    ASSERT_EQ(sat_anim_vertex_gouraud(&a, &st, out, 2), SAT_ERR_UNSUPPORTED);
}

static void decode_capacity_and_malformed() {
    sat_animated_model_asset_t a = make_asset(true);
    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 0), SAT_OK);
    sat_vec3_t out[2] = {{-7, -7, -7}, {-7, -7, -7}};
    ASSERT_EQ(sat_anim_decode(&a, &st, out, 1), SAT_ERR_CAPACITY);
    ASSERT_EQ(out[0].x, -7); /* writes nothing on capacity failure */
    ASSERT_EQ(sat_anim_decode(&a, &st, nullptr, 2), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_anim_decode(nullptr, &st, out, 2), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_anim_decode(&a, nullptr, out, 2), SAT_ERR_INVALID_ARG);
    sat_anim_state_t bad = st;
    bad.frame = 9;
    ASSERT_EQ(sat_anim_decode(&a, &bad, out, 2), SAT_ERR_INVALID_ARG);
}

static void wide_sort_draws_big_meshes() {
    /* 300 faces over 4 shared verts: exercises the 16-bit sort path. */
    static sat_vec3_t verts[4] = {{-65536, 0, 0}, {65536, 0, 0}, {65536, 65536, 0}, {-65536, 65536, 0}};
    static uint16_t indices[300 * 4];
    static uint16_t order16[300];
    static uint32_t depth[300];
    static uint16_t face_tex[300];
    for (int f = 0; f < 300; ++f) {
        indices[f * 4 + 0] = 0;
        indices[f * 4 + 1] = 1;
        indices[f * 4 + 2] = 2;
        indices[f * 4 + 3] = 3;
        face_tex[f] = SAT_MESH_TEXTURE_NONE;
    }
    sat_mesh_t mesh = {};
    ASSERT_EQ(sat_mesh_init(&mesh, verts, 4, indices, 300), SAT_OK);
    mesh.vertex_count = 4;
    mesh.face_count = 300;
    sat_mat4_t vp = {};
    sat_vec3_t eye = {0, 0, 65536 * 10};
    sat_mesh_draw_t draw = {};
    draw.view_proj = &vp;
    draw.eye = eye;
    draw.color = 0x8000u;
    draw.face_texture_indices = face_tex;
    draw.textures = nullptr;
    draw.texture_count = 0;
    draw.flags = SAT_MESH_SORT;
    draw.order = nullptr;
    draw.depth = depth;
    draw.order16 = order16;
    /* Degenerate view-projection draws nothing but must accept the mesh. */
    sat_result_t st = sat_draw_mesh(&mesh, &draw);
    ASSERT_TRUE(st == SAT_OK || st == SAT_ERR_UNSUPPORTED);
    /* Missing wide scratch is a clean error, not a crash. */
    draw.order16 = nullptr;
    ASSERT_EQ(sat_draw_mesh(&mesh, &draw), SAT_ERR_INVALID_ARG);
    /* sort16 orders farthest-first like the 8-bit form. */
    static uint16_t keys_order[4] = {0, 1, 2, 3};
    static uint32_t keys[4] = {10u, 30u, 20u, 40u};
    saturn::core::render3d::sort_indices16_desc(keys_order, keys, 4);
    ASSERT_EQ(keys_order[0], 3u);
    ASSERT_EQ(keys_order[1], 1u);
    ASSERT_EQ(keys_order[2], 2u);
    ASSERT_EQ(keys_order[3], 0u);
}

static void legacy_small_mesh_unaffected() {
    sat_vec3_t verts[4] = {{-65536, 0, 0}, {65536, 0, 0}, {65536, 65536, 0}, {-65536, 65536, 0}};
    uint16_t indices[8] = {0, 1, 2, 3, 0, 1, 2, 3};
    sat_mesh_t mesh = {};
    ASSERT_EQ(sat_mesh_init(&mesh, verts, 4, indices, 2), SAT_OK);
    mesh.vertex_count = 4;
    mesh.face_count = 2;
    uint8_t order[2] = {0, 1};
    uint32_t depth[2] = {0, 0};
    uint16_t face_tex[2] = {SAT_MESH_TEXTURE_NONE, SAT_MESH_TEXTURE_NONE};
    sat_mat4_t vp = {};
    sat_vec3_t eye = {0, 0, 65536 * 10};
    sat_mesh_draw_t draw = {};
    draw.view_proj = &vp;
    draw.eye = eye;
    draw.color = 0x8000u;
    draw.face_texture_indices = face_tex;
    draw.flags = SAT_MESH_SORT;
    draw.order = order;
    draw.depth = depth;
    draw.order16 = nullptr;
    sat_result_t st = sat_draw_mesh(&mesh, &draw);
    ASSERT_TRUE(st == SAT_OK || st == SAT_ERR_UNSUPPORTED);
}

static void decode_feeds_mesh_and_bind() {
    sat_animated_model_asset_t a = make_asset(true);
    sat_anim_state_t st = {};
    ASSERT_EQ(sat_anim_state_init(&st, &a, 0), SAT_OK);
    static sat_vec3_t verts[2];
    static uint16_t indices[4];
    sat_mesh_t mesh = {};
    ASSERT_EQ(sat_mesh_init(&mesh, verts, 2, indices, 1), SAT_OK);
    ASSERT_EQ(sat_model_copy_to_mesh(a.model, &mesh), SAT_OK);
    /* Decode the pose over the copied bind vertices. */
    ASSERT_EQ(sat_anim_decode(&a, &st, mesh.vertices, mesh.vertex_cap), SAT_OK);
    sat_vdp1_texture_t tex[1] = {};
    tex[0].valid = 1;
    sat_mat4_t vp = {};
    sat_vec3_t eye = {0, 0, 65536};
    sat_mesh_draw_t draw = {};
    ASSERT_EQ(sat_model_bind_draw_ex(
        a.model, &mesh, tex, 1, &vp, &eye, 0x8000u, nullptr, 0, 0,
        nullptr, nullptr, nullptr, &draw), SAT_OK);
    ASSERT_TRUE(draw.order16 == nullptr);
}

int main() {
    valid_descriptor_passes();
    malformed_descriptors_rejected();
    clip_duration_matches_rate();
    init_set_reset();
    pause_blocks_advance();
    ntsc_advance_steps_two_vblanks_per_frame();
    pal_fractional_accumulator_wraps_cleanly();
    loop_wraps_and_once_holds();
    decode_matches_quantization_contract();
    decode_extremes_and_bias();
    decode_capacity_and_malformed();
    divide_free_decode_is_exact();
    face_colors_follow_baked_shades();
    vertex_gouraud_follows_baked_levels();
    wide_sort_draws_big_meshes();
    legacy_small_mesh_unaffected();
    decode_feeds_mesh_and_bind();
    printf("PASS: test_anim3d_logic.cpp (17 tests)\n");
    return 0;
}
