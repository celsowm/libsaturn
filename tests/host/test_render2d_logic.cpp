#include <cstdio>
#include <cstdlib>

#include "src/core/render2d_logic.hpp"
#include "src/core/render2d_runtime.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static int abs_i(int v) { return v < 0 ? -v : v; }

int main() {
    using namespace saturn::core;

    sat_draw_params_t params = sat_draw_params_default();
    OK(validate_render2d_params(&params) == SAT_OK);

    params.flip = 4u;
    OK(validate_render2d_params(&params) == SAT_ERR_INVALID_ARG);
    params = sat_draw_params_default();
    params.blend_mode = SAT_BLEND_ALPHA;
    OK(validate_render2d_params(&params) == SAT_ERR_UNSUPPORTED);
    params = sat_draw_params_default();
    params.tint.r = 254u;
    OK(validate_render2d_params(&params) == SAT_ERR_UNSUPPORTED);

    const sat_rect_t dst{10, 20, 8u, 4u};
    Render2DDestination resolved{};
    OK(resolve_render2d_destination(&dst, 320u, 224u, 8u, 4u, &resolved) == SAT_OK);
    OK(resolved.x0 == -150 && resolved.y0 == -92);
    OK(resolved.x1 == -143 && resolved.y1 == -89);
    OK(!resolved.scaled);
    OK(resolve_render2d_destination(&dst, 320u, 224u, 16u, 4u, &resolved) == SAT_OK);
    OK(resolved.scaled);

    params = sat_draw_params_default();
    Render2DQuad quad{};
    OK(resolve_render2d_quad(&dst, 320u, 224u, params, &quad) == SAT_OK);
    OK(quad.x[0] == -150 && quad.y[0] == -92);
    OK(quad.x[1] == -143 && quad.y[1] == -92);
    OK(quad.x[2] == -143 && quad.y[2] == -89);
    OK(quad.x[3] == -150 && quad.y[3] == -89);

    params.flip = SAT_FLIP_X;
    OK(resolve_render2d_quad(&dst, 320u, 224u, params, &quad) == SAT_OK);
    OK(quad.x[0] == -143 && quad.y[0] == -92);
    OK(quad.x[1] == -150 && quad.y[1] == -92);
    OK(quad.x[2] == -150 && quad.y[2] == -89);
    OK(quad.x[3] == -143 && quad.y[3] == -89);

    params = sat_draw_params_default();
    params.flip = SAT_FLIP_Y;
    OK(resolve_render2d_quad(&dst, 320u, 224u, params, &quad) == SAT_OK);
    OK(quad.x[0] == -150 && quad.y[0] == -89);
    OK(quad.x[1] == -143 && quad.y[1] == -89);
    OK(quad.x[2] == -143 && quad.y[2] == -92);
    OK(quad.x[3] == -150 && quad.y[3] == -92);

    const sat_rect_t rotated_dst{100, 50, 10u, 6u};
    params = sat_draw_params_default();
    params.center.x = 5;
    params.center.y = 3;
    params.rotation = static_cast<sat_fx16_t>(90 << 16);
    OK(resolve_render2d_quad(&rotated_dst, 320u, 224u, params, &quad) == SAT_OK);
    OK(abs_i(quad.x[0] - (-52)) <= 1 && abs_i(quad.y[0] - (-64)) <= 1);
    OK(abs_i(quad.x[1] - (-52)) <= 1 && abs_i(quad.y[1] - (-55)) <= 1);
    OK(abs_i(quad.x[2] - (-57)) <= 1 && abs_i(quad.y[2] - (-55)) <= 1);
    OK(abs_i(quad.x[3] - (-57)) <= 1 && abs_i(quad.y[3] - (-64)) <= 1);

    params = sat_draw_params_default();
    OK(resolve_render2d_quad(&dst, 320u, 224u, params, &quad) == SAT_OK);
    sat_camera2d_t camera = sat_camera2d_default();
    camera.offset_x = static_cast<sat_fx16_t>(160 << 16);
    camera.offset_y = static_cast<sat_fx16_t>(112 << 16);
    camera.target_x = static_cast<sat_fx16_t>(10 << 16);
    camera.target_y = static_cast<sat_fx16_t>(20 << 16);
    OK(apply_render2d_camera(&quad, 320u, 224u, camera) == SAT_OK);
    OK(quad.x[0] == 0 && quad.y[0] == 0);
    OK(quad.x[1] == 7 && quad.y[1] == 0);
    OK(quad.x[2] == 7 && quad.y[2] == 3);
    OK(quad.x[3] == 0 && quad.y[3] == 3);

    camera.zoom = static_cast<sat_fx16_t>(2 << 16);
    OK(resolve_render2d_quad(&dst, 320u, 224u, params, &quad) == SAT_OK);
    OK(apply_render2d_camera(&quad, 320u, 224u, camera) == SAT_OK);
    OK(quad.x[0] == 0 && quad.y[0] == 0);
    OK(quad.x[1] == 14 && quad.y[1] == 0);
    OK(quad.x[2] == 14 && quad.y[2] == 6);

    camera = sat_camera2d_default();
    camera.zoom = 0;
    OK(apply_render2d_camera(&quad, 320u, 224u, camera) == SAT_ERR_INVALID_ARG);

    Render2DRuntime runtime{};
    render2d_runtime_reset(runtime);
    OK(runtime.depth == 0u);
    OK(render2d_camera_is_identity(runtime.current.camera));
    for (uint16_t i = 0u; i < kRender2DStackCapacity; ++i) {
        OK(render2d_runtime_push(runtime) == SAT_OK);
    }
    OK(render2d_runtime_push(runtime) == SAT_ERR_CAPACITY);
    sat_camera2d_t moved = sat_camera2d_default();
    moved.target_x = static_cast<sat_fx16_t>(42 << 16);
    OK(render2d_runtime_set_camera(runtime, moved) == SAT_OK);
    OK(runtime.current.camera.target_x == moved.target_x);
    OK(render2d_runtime_pop(runtime) == SAT_OK);
    OK(render2d_camera_is_identity(runtime.current.camera));
    while (runtime.depth != 0u) OK(render2d_runtime_pop(runtime) == SAT_OK);
    OK(render2d_runtime_pop(runtime) == SAT_ERR_INVALID_ARG);
    moved.zoom = 0;
    OK(render2d_runtime_set_camera(runtime, moved) == SAT_ERR_INVALID_ARG);

    std::puts("render2d logic: OK");
    return 0;
}
