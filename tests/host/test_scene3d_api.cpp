#include <cstdio>

#include "saturn/scene3d.h"
#include "saturn/vdp1.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

extern "C" sat_result_t sat_palette_upload_indexed8(
    const uint16_t*, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_tex_upload_indexed8_pixels(
    sat_vdp1_texture_t*, const uint8_t*, uint16_t, uint16_t, uint16_t) {
    return SAT_OK;
}

int main() {
    const sat_vec3_t eye = {0, 0, sat_fx16_from_int(5)};
    const sat_vec3_t target = {0, 0, 0};
    const sat_vec3_t up = {0, sat_fx16_from_int(1), 0};
    sat_camera3d_t camera{};
    OK(sat_camera3d_init(&camera, &eye, &target, &up,
        sat_fx16_from_int(60), sat_fx16_from_int(4) / sat_fx16_from_int(3),
        sat_fx16_from_int(1), sat_fx16_from_int(100)) == SAT_OK);
    OK(camera.view_proj.m[0] != 0);

    sat_model_transform3d_t transform{};
    sat_model_transform3d_identity(&transform);
    transform.position.x = sat_fx16_from_int(2);
    sat_mat4_t model_matrix{};
    OK(sat_model_transform3d_matrix(&transform, &model_matrix) == SAT_OK);
    OK(model_matrix.m[3] == sat_fx16_from_int(2));

    std::puts("scene3d camera/transform api: OK");
    return 0;
}
