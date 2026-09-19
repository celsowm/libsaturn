#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include "examples/voxel_display_probe/voxel_display_math.h"
#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

int main() {
    uint16_t params[48];
    for (auto& value : params) value = 0xFFFFu;
    voxel_probe_rbg0_build_params(params);
    OK(params[8] == 1u && params[10] == 1u);
    OK(params[14] == 1u && params[22] == 1u);
    OK(params[39] == 0x8000u && params[41] == 0x8000u);
    OK(params[0] == 0u && params[2] == 0u && params[38] == 0u);
    /* Pure identity rotation+half-scale coordinates at all four corners. */
    for (uint32_t screen_y : {0u, 1u, 222u, 223u}) {
        for (uint32_t screen_x : {0u, 1u, 318u, 319u}) {
            const uint32_t sample_x = (screen_x * params[10] +
                                        screen_y * params[6]) / 2u;
            const uint32_t sample_y = (screen_x * params[12] +
                                        screen_y * params[8]) / 2u;
            OK(sample_x < VOXEL_PROBE_SRC_W);
            OK(sample_y < VOXEL_PROBE_SRC_H);
            OK(sample_x == screen_x / 2u && sample_y == screen_y / 2u);
        }
    }
    OK(voxel_probe_rbg0_row_offset(0u) == 0u);
    OK(voxel_probe_rbg0_row_offset(1u) == 256u);
    OK(voxel_probe_rbg0_row_offset(111u) + VOXEL_PROBE_SRC_W / 2u <= 0x10000u);
    uint8_t pixels[VOXEL_PROBE_SRC_W]{};
    uint16_t packed[VOXEL_PROBE_SRC_W / 2u]{};
    pixels[0] = 0x12u; pixels[1] = 0x34u;
    pixels[158] = 0x56u; pixels[159] = 0x78u;
    voxel_probe_rbg0_pack_row(pixels, VOXEL_PROBE_SRC_W, packed);
    OK(packed[0] == 0x1234u && packed[79] == 0x5678u);
    OK(packed[1] == 0u && packed[78] == 0u);
    std::puts("voxel RBG0 display probe math: OK");
    return 0;
}
