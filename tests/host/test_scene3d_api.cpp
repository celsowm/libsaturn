#include <cstdio>
#include <cstdlib>

#include "saturn/scene3d.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static int g_draw_calls;

extern "C" sat_result_t sat_mesh_init(
    sat_mesh_t* mesh,
    sat_vec3_t* vertices,
    uint16_t vertex_cap,
    uint16_t* indices,
    uint16_t face_cap
) {
    if (mesh == nullptr || vertices == nullptr || indices == nullptr ||
        vertex_cap == 0u || face_cap == 0u) return SAT_ERR_INVALID_ARG;
    mesh->vertices = vertices;
    mesh->indices = indices;
    mesh->vertex_cap = vertex_cap;
    mesh->vertex_count = 0u;
    mesh->face_cap = face_cap;
    mesh->face_count = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_mesh_transform(sat_mesh_t*, const sat_mat4_t*) {
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_mesh(const sat_mesh_t* mesh, const sat_mesh_draw_t* params) {
    OK(mesh != nullptr && params != nullptr);
    ++g_draw_calls;
    return SAT_OK;
}

extern "C" sat_result_t sat_palette_upload_indexed8(const uint16_t*, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_tex_upload_indexed8_pixels(
    sat_vdp1_texture_t*, const uint8_t*, uint16_t, uint16_t, uint16_t) { return SAT_OK; }

int main() {
    const sat_vec3_t eye = {0, 0, sat_fx16_from_int(5)};
    const sat_vec3_t target = {0, 0, 0};
    const sat_vec3_t up = {0, sat_fx16_from_int(1), 0};
    sat_camera3d_t camera{};
    OK(sat_camera3d_init(
        &camera, &eye, &target, &up, sat_fx16_from_int(60),
        sat_fx16_from_int(4) / sat_fx16_from_int(3), sat_fx16_from_int(1),
        sat_fx16_from_int(100)) == SAT_OK);
    OK(camera.view_proj.m[0] != 0);
    camera.target.x = sat_fx16_from_int(1);
    OK(sat_camera3d_update(&camera) == SAT_OK);

    sat_model_transform3d_t transform{};
    sat_model_transform3d_identity(&transform);
    OK(transform.scale.x == SAT_FX16_ONE);
    transform.position.x = sat_fx16_from_int(2);
    sat_mat4_t model_matrix{};
    OK(sat_model_transform3d_matrix(&transform, &model_matrix) == SAT_OK);
    OK(model_matrix.m[3] == sat_fx16_from_int(2));

    sat_vec3_t vertices[4] = {};
    uint16_t indices[4] = {};
    uint8_t order[1] = {};
    uint32_t depth[1] = {};
    sat_projected_vertex_t screen[4] = {};
    sat_scene3d_t scene{};
    OK(sat_scene3d_init(
        &scene, vertices, 4u, indices, 1u, order, nullptr, depth, screen) == SAT_OK);
    OK(sat_scene3d_draw_model(&scene, nullptr, &transform, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_scene3d_begin(&scene, &camera) == SAT_OK);
    const sat_vec3_t model_vertices[4] = {
        {-SAT_FX16_ONE, -SAT_FX16_ONE, 0},
        { SAT_FX16_ONE, -SAT_FX16_ONE, 0},
        { SAT_FX16_ONE,  SAT_FX16_ONE, 0},
        {-SAT_FX16_ONE,  SAT_FX16_ONE, 0},
    };
    const uint16_t model_indices[4] = {0u, 1u, 2u, 3u};
    const uint16_t face_textures[1] = {SAT_MESH_TEXTURE_NONE};
    const sat_model_asset_t model_asset = {
        model_vertices, 4u, model_indices, 1u, face_textures,
        nullptr, 0u, nullptr, 0u, 0u, 0u, nullptr, 0u, nullptr
    };
    const sat_scene3d_model_params_t params = {nullptr, 0u, 0x7FFFu, 0, 0u, nullptr, nullptr};
    OK(sat_scene3d_draw_model(&scene, &model_asset, &transform, &params) == SAT_OK);
    OK(g_draw_calls == 1);
    OK(sat_scene3d_end(&scene) == SAT_OK);
    OK(sat_scene3d_end(&scene) == SAT_ERR_INVALID_ARG);
    std::puts("scene3d api: OK");
    return 0;
}
