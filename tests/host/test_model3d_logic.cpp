/* test_model3d_logic.cpp -- host tests for compiled-model descriptors. */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "saturn/model3d.h"
#include "src/core/model3d_logic.hpp"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

namespace {

sat_vec3_t kVerts[4] = {{0, 0, 0}, {0, 65536, 0}, {65536, 65536, 0}, {65536, 0, 0}};
uint16_t kIndices[8] = {0, 1, 2, 3, 0, 1, 2, 3};
uint16_t kFaceTex[2] = {0u, SAT_MESH_TEXTURE_NONE};
uint8_t kPixels[16 * 8] = {};
uint16_t kPalette[256] = {};
sat_model_texture_asset_t kTex[1] = {{kPixels, 16, 8, 0, 0, 16u * 8u}};

sat_model_asset_t make_valid() {
    sat_model_asset_t a = {};
    a.vertices = kVerts;
    a.vertex_count = 4;
    a.indices = kIndices;
    a.face_count = 2;
    a.face_texture_indices = kFaceTex;
    a.textures = kTex;
    a.texture_count = 1;
    a.palettes_rgb555 = kPalette;
    a.palette_count = 1;
    a.palette_base = 1;
    return a;
}

}  // namespace

/* Stubbed VDP1 upload layer for sat_model_upload_textures. */
extern "C" sat_result_t sat_palette_upload_indexed8(const uint16_t*, uint16_t) {
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_world_polygon(
    const sat_mat4_t*, const sat_quad3_t*, uint16_t) {
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_world_sprite(
    const sat_mat4_t*, const sat_quad3_t*,
    const sat_texture_t*, uint16_t, uint16_t) {
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
    const sat_quad2_t*, const sat_texture_t*, uint16_t, uint16_t) {
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

static int g_tex_calls = 0;
static sat_result_t g_tex_status = SAT_OK;

extern "C" sat_result_t sat_tex_upload_indexed8_pixels(
    sat_texture_t* out, const uint8_t*, uint16_t w, uint16_t h, uint16_t pal) {
    ++g_tex_calls;
    if (g_tex_status != SAT_OK) {
        return g_tex_status;
    }
    out->srca = 1;
    out->width = w;
    out->height = h;
    out->palette = pal;
    out->valid = 1;
    out->reserved = 0;
    return SAT_OK;
}

static void valid_descriptor_passes() {
    sat_model_asset_t a = make_valid();
    ASSERT_EQ(sat_model_validate(&a), SAT_OK);
}

static void null_and_empty_rejected() {
    ASSERT_EQ(sat_model_validate(nullptr), SAT_ERR_INVALID_ARG);
    sat_model_asset_t a = make_valid();
    sat_model_asset_t bad = a;
    bad.vertices = nullptr;
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);
    bad = a;
    bad.face_count = 0;
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);
}

/* A solid-color asset carries a shade palette instead of textures, and
 * binds for drawing without a texture table. */
static void solid_color_model_needs_no_textures() {
    sat_model_asset_t a = make_valid();
    static uint16_t none[2] = {SAT_MESH_TEXTURE_NONE, SAT_MESH_TEXTURE_NONE};
    static uint16_t shades[4] = {0x8000u, 0x801Fu, 0x83E0u, 0xFC00u};
    a.face_texture_indices = none;
    a.textures = nullptr;
    a.texture_count = 0;
    a.palettes_rgb555 = nullptr;
    a.palette_count = 0;
    a.shade_palette_rgb555 = shades;
    a.shade_palette_count = 4;
    ASSERT_EQ(sat_model_validate(&a), SAT_OK);

    sat_model_asset_t bad = a;
    bad.shade_palette_rgb555 = nullptr;
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);
    bad = a;
    bad.shade_palette_count = 257;
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);
    bad = a;
    bad.texture_count = 1; /* claims textures but has no table */
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);

    static uint8_t base[2] = {1, 3};
    a.face_base_shades = base;
    ASSERT_EQ(sat_model_validate(&a), SAT_OK);
    uint16_t colors[2] = {0, 0};
    ASSERT_EQ(sat_model_face_base_colors(&a, colors, 2), SAT_OK);
    ASSERT_EQ(colors[0], 0x801Fu);
    ASSERT_EQ(colors[1], 0xFC00u);
    ASSERT_EQ(sat_model_face_base_colors(&a, colors, 1), SAT_ERR_CAPACITY);
    base[1] = 4; /* past the palette */
    ASSERT_EQ(sat_model_validate(&a), SAT_ERR_INVALID_ARG);
    base[1] = 3;
    bad = a;
    bad.face_base_shades = nullptr;
    ASSERT_EQ(sat_model_face_base_colors(&bad, colors, 2), SAT_ERR_UNSUPPORTED);

    sat_vec3_t verts[4];
    uint16_t idx[8];
    sat_mesh_t mesh;
    ASSERT_EQ(sat_mesh_init(&mesh, verts, 4, idx, 2), SAT_OK);
    ASSERT_EQ(sat_model_copy_to_mesh(&a, &mesh), SAT_OK);
    sat_mat4_t vp = {};
    sat_vec3_t eye = {0, 0, 65536};
    sat_mesh_draw_t draw = {};
    ASSERT_EQ(sat_model_bind_draw(
        &a, &mesh, nullptr, 0, &vp, &eye, 0x8000u, nullptr, 0, 0, nullptr, nullptr, &draw),
        SAT_OK);
    ASSERT_TRUE(draw.textures == nullptr);
    ASSERT_EQ(draw.texture_count, 0u);
}

static void face_vertex_bounds_checked() {
    sat_model_asset_t a = make_valid();
    static uint16_t bad_idx[8] = {0, 1, 2, 9, 0, 1, 2, 3};
    sat_model_asset_t bad = a;
    bad.indices = bad_idx;
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);
}

static void face_texture_mapping_checked() {
    sat_model_asset_t a = make_valid();
    static uint16_t bad_map[2] = {0u, 7u};
    sat_model_asset_t bad = a;
    bad.face_texture_indices = bad_map;
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);
}

static void texture_dims_and_pixel_count_checked() {
    sat_model_asset_t a = make_valid();
    static sat_model_texture_asset_t bad_tex[1] = {{kPixels, 12, 8, 0, 0, 12u * 8u}};
    sat_model_asset_t bad = a;
    bad.textures = bad_tex;
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);
    static sat_model_texture_asset_t bad_tex2[1] = {{kPixels, 16, 8, 0, 0, 7u}};
    bad.textures = bad_tex2;
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);
    static sat_model_texture_asset_t bad_tex3[1] = {{kPixels, 512, 8, 0, 0, 512u * 8u}};
    bad.textures = bad_tex3;
    ASSERT_EQ(sat_model_validate(&bad), SAT_ERR_INVALID_ARG);
}

static void copy_to_mesh_requires_capacity() {
    sat_model_asset_t a = make_valid();
    sat_vec3_t verts[4];
    uint16_t idx[8];
    sat_mesh_t mesh;
    ASSERT_EQ(sat_mesh_init(&mesh, verts, 4, idx, 2), SAT_OK);
    ASSERT_EQ(sat_model_copy_to_mesh(&a, &mesh), SAT_OK);
    ASSERT_EQ(mesh.vertex_count, 4u);
    ASSERT_EQ(mesh.face_count, 2u);
    ASSERT_EQ(mesh.vertices[2].x, kVerts[2].x);

    sat_vec3_t small_v[2];
    uint16_t small_i[4];
    sat_mesh_t small;
    ASSERT_EQ(sat_mesh_init(&small, small_v, 2, small_i, 1), SAT_OK);
    ASSERT_EQ(sat_model_copy_to_mesh(&a, &small), SAT_ERR_CAPACITY);
}

static void upload_requires_output_capacity() {
    sat_model_asset_t a = make_valid();
    sat_texture_t out[1];
    g_tex_calls = 0;
    g_tex_status = SAT_OK;
    ASSERT_EQ(sat_model_upload_textures(&a, out, 1), SAT_OK);
    ASSERT_EQ(g_tex_calls, 1);
    ASSERT_EQ(out[0].valid, 1u);
    ASSERT_EQ(sat_model_upload_textures(&a, out, 0), SAT_ERR_CAPACITY);
    g_tex_status = SAT_ERR_CAPACITY;
    ASSERT_EQ(sat_model_upload_textures(&a, out, 1), SAT_ERR_CAPACITY);
    g_tex_status = SAT_OK;
}

static void bounds_and_center_derived() {
    sat_model_asset_t a = make_valid();
    sat_vec3_t mn, mx, c;
    ASSERT_EQ(sat_model_compute_bounds(&a, &mn, &mx), SAT_OK);
    ASSERT_EQ(mn.x, 0);
    ASSERT_EQ(mx.x, 65536);
    ASSERT_EQ(mx.y, 65536);
    ASSERT_EQ(sat_model_compute_center(&a, &c), SAT_OK);
    ASSERT_EQ(c.x, 32768);
    ASSERT_EQ(c.y, 32768);
}

static void byte_and_vram_estimates() {
    sat_model_asset_t a = make_valid();
    ASSERT_EQ(sat_model_texture_bytes(&a), 128u);
    /* 128 bytes already 8-aligned. */
    ASSERT_EQ(sat_model_vram_estimate_bytes(&a), 128u);
}

static void bind_draw_fills_mesh_draw() {
    sat_model_asset_t a = make_valid();
    sat_vec3_t verts[4];
    uint16_t idx[8];
    sat_mesh_t mesh;
    ASSERT_EQ(sat_mesh_init(&mesh, verts, 4, idx, 2), SAT_OK);
    ASSERT_EQ(sat_model_copy_to_mesh(&a, &mesh), SAT_OK);
    sat_texture_t tex[1] = {};
    tex[0].valid = 1;
    sat_mat4_t vp = {};
    sat_vec3_t eye = {0, 0, 65536};
    sat_mesh_draw_t draw = {};
    ASSERT_EQ(sat_model_bind_draw(
        &a, &mesh, tex, 1, &vp, &eye, 0x8000u, nullptr, 0, 0, nullptr, nullptr, &draw),
        SAT_OK);
    ASSERT_TRUE(draw.face_texture_indices == a.face_texture_indices);
    ASSERT_TRUE(draw.textures == tex);
    /* SORT without scratch is rejected. */
    ASSERT_EQ(sat_model_bind_draw(
        &a, &mesh, tex, 1, &vp, &eye, 0, nullptr, 0, SAT_MESH_SORT, nullptr, nullptr, &draw),
        SAT_ERR_INVALID_ARG);
}

int main() {
    valid_descriptor_passes();
    null_and_empty_rejected();
    solid_color_model_needs_no_textures();
    face_vertex_bounds_checked();
    face_texture_mapping_checked();
    texture_dims_and_pixel_count_checked();
    copy_to_mesh_requires_capacity();
    upload_requires_output_capacity();
    bounds_and_center_derived();
    byte_and_vram_estimates();
    bind_draw_fills_mesh_draw();
    printf("PASS: test_model3d_logic.cpp (11 tests)\n");
    return 0;
}
