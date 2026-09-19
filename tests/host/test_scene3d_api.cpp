#include <cstdio>
#include <cstdlib>

#include "saturn/scene3d.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static int g_draw_calls;
static int g_painter_order[32];
static int g_painter_count;
struct PainterCapture {
    sat_scene3d_queue_t* queue;
    int id;
    sat_result_t error;
};
static sat_result_t capture_painter(void* user, const sat_camera3d_t*) {
    PainterCapture* cap=static_cast<PainterCapture*>(user);
    OK(g_painter_count<32);
    g_painter_order[g_painter_count++]=cap->id;
    /* Mutating/re-entering the queue during a callback is illegal. */
    const sat_vec3_t origin={0,0,0};
    OK(sat_scene3d_queue_submit_draw(
        cap->queue,&origin,0u,capture_painter,user)==SAT_ERR_INVALID_ARG);
    OK(sat_scene3d_queue_flush(cap->queue)==SAT_ERR_INVALID_ARG);
    return cap->error;
}


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
    /* Same-world-points / opposite-orbit regression: actor ordering changes
     * with camera, never with submission order or world Z alone. */
    sat_scene3d_queue_item_t storage[5]={};
    sat_scene3d_queue_t painter{};
    OK(sat_scene3d_queue_init(nullptr,storage,5u)==SAT_ERR_INVALID_ARG);
    OK(sat_scene3d_queue_init(&painter,nullptr,5u)==SAT_ERR_INVALID_ARG);
    OK(sat_scene3d_queue_init(&painter,storage,0u)==SAT_ERR_INVALID_ARG);
    OK(sat_scene3d_queue_init(&painter,storage,5u)==SAT_OK);
    PainterCapture caps[5]={
        {&painter,10,SAT_OK}, {&painter,20,SAT_OK},
        {&painter,30,SAT_OK}, {&painter,40,SAT_OK},
        {&painter,50,SAT_OK}
    };
    const sat_vec3_t far_actor={0,0,0};
    const sat_vec3_t near_actor={0,0,sat_fx16_from_int(7)};
    const sat_vec3_t same_actor={0,0,sat_fx16_from_int(7)};
    OK(sat_scene3d_queue_submit_draw(&painter,&far_actor,0u,capture_painter,&caps[0])
       ==SAT_ERR_INVALID_ARG);
    camera.eye={0,0,sat_fx16_from_int(10)};
    camera.target={0,0,0};
    OK(sat_camera3d_update(&camera)==SAT_OK);
    OK(sat_scene3d_queue_begin(&painter,&camera)==SAT_OK);
    OK(sat_scene3d_queue_begin(&painter,&camera)==SAT_ERR_INVALID_ARG);
    sat_fx16_t camera_depth=0;
    OK(sat_scene3d_queue_depth(&painter,&far_actor,&camera_depth)==SAT_OK);
    OK(camera_depth==sat_fx16_from_int(10));
    OK(sat_scene3d_queue_submit_draw(&painter,&near_actor,1u,capture_painter,&caps[1])==SAT_OK);
    OK(sat_scene3d_queue_submit_draw(&painter,&far_actor,1u,capture_painter,&caps[0])==SAT_OK);
    OK(sat_scene3d_queue_submit_draw(&painter,&same_actor,1u,capture_painter,&caps[2])==SAT_OK);
    /* Pass 0 is an explicit background dependency, independent of depth. */
    OK(sat_scene3d_queue_submit_draw(&painter,&near_actor,0u,capture_painter,&caps[3])==SAT_OK);
    OK(sat_scene3d_queue_submit_draw(&painter,&far_actor,1u,capture_painter,&caps[4])==SAT_OK);
    OK(sat_scene3d_queue_submit_draw(&painter,&far_actor,1u,capture_painter,&caps[4])
       ==SAT_ERR_CAPACITY);
    g_painter_count=0;
    OK(sat_scene3d_queue_flush(&painter)==SAT_OK);
    OK(g_painter_count==5);
    OK(g_painter_order[0]==40);
    OK(g_painter_order[1]==10 && g_painter_order[2]==50);
    OK(g_painter_order[3]==20 && g_painter_order[4]==30);
    OK(sat_scene3d_queue_flush(&painter)==SAT_ERR_INVALID_ARG);
    /* Flip the camera: the same two actors reverse depth order. */
    camera.eye={0,0,-sat_fx16_from_int(10)};
    camera.target={0,0,0};
    OK(sat_camera3d_update(&camera)==SAT_OK);
    OK(sat_scene3d_queue_begin(&painter,&camera)==SAT_OK);
    OK(sat_scene3d_queue_submit_draw(&painter,&near_actor,0u,capture_painter,&caps[1])==SAT_OK);
    OK(sat_scene3d_queue_submit_draw(&painter,&far_actor,0u,capture_painter,&caps[0])==SAT_OK);
    g_painter_count=0;
    OK(sat_scene3d_queue_flush(&painter)==SAT_OK);
    OK(g_painter_count==2 && g_painter_order[0]==20 && g_painter_order[1]==10);
    /* Pitch distinguishes points with equal X/Z but different Y. */
    camera.eye={0,sat_fx16_from_int(20),sat_fx16_from_int(10)};
    camera.target={0,0,0};
    OK(sat_camera3d_update(&camera)==SAT_OK);
    const sat_vec3_t lower={0,0,0};
    const sat_vec3_t upper={0,sat_fx16_from_int(5),0};
    OK(sat_scene3d_queue_begin(&painter,&camera)==SAT_OK);
    OK(sat_scene3d_queue_submit_draw(&painter,&upper,0u,capture_painter,&caps[1])==SAT_OK);
    OK(sat_scene3d_queue_submit_draw(&painter,&lower,0u,capture_painter,&caps[0])==SAT_OK);
    g_painter_count=0;
    OK(sat_scene3d_queue_flush(&painter)==SAT_OK);
    OK(g_painter_count==2 && g_painter_order[0]==10 && g_painter_order[1]==20);
    /* Stable sorting for equal depth across re-used frames and an explicit
     * error guarantee (stop once, close, allow a fresh begin). */
    OK(sat_scene3d_queue_begin(&painter,&camera)==SAT_OK);
    caps[0].error=SAT_ERR_IO;
    OK(sat_scene3d_queue_submit_draw(&painter,&upper,0u,capture_painter,&caps[0])==SAT_OK);
    OK(sat_scene3d_queue_submit_draw(&painter,&upper,0u,capture_painter,&caps[1])==SAT_OK);
    g_painter_count=0;
    OK(sat_scene3d_queue_flush(&painter)==SAT_ERR_IO);
    OK(g_painter_count==1 && g_painter_order[0]==10);
    caps[0].error=SAT_OK;
    OK(sat_scene3d_queue_begin(&painter,&camera)==SAT_OK);
    /* Deferred compiled models retain existing immediate draw behavior. */
    const int previous_draw_calls=g_draw_calls;
    const sat_vec3_t local_center={0,0,0};
    transform.position={0,0,0};
    OK(sat_scene3d_queue_submit_model(
        &painter,&local_center,0u,&scene,&model_asset,&transform,&params)==SAT_OK);
    g_painter_count=0;
    OK(sat_scene3d_queue_flush(&painter)==SAT_OK);
    OK(g_draw_calls==previous_draw_calls+1 && !scene.active);
    std::puts("scene3d api: OK");
    return 0;
}
