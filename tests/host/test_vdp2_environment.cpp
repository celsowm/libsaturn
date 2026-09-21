#include <cstdio>

#include "saturn/vdp2_environment.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

extern "C" sat_result_t sat_vdp2_vram_write_words(uint32_t, const uint16_t*, uint32_t) {
    return SAT_OK;
}
extern "C" sat_result_t sat_vdp2_rbg0_mode7_init(
    const sat_vdp2_rbg0_mode7_config_t*) { return SAT_OK; }

int main() {
    const sat_vdp2_rbg0_ground_config_t ground =
        {512u, 256u, 160u, 96u, 96u, 8u, 96u, 0x12000u};
    const sat_vdp2_rbg0_mode7_config_t mode =
        {SAT_VDP2_RBG0_BITMAP_512x256, SAT_VDP2_COLOR_MODE_256,
         0u, 0x10000u, 0u, 5u, 7u};
    OK(sat_vdp2_ground_environment_validate_layout(
        &ground, &mode, 224u, SAT_VDP2_VRAM_WORD_CAPACITY, 0u, 512u, 2u) == SAT_OK);
    OK(sat_vdp2_ground_environment_validate_layout(
        &ground, &mode, 224u, SAT_VDP2_VRAM_WORD_CAPACITY, 0u, 512u, 8u)
        == SAT_ERR_INVALID_ARG);
    const sat_vdp2_rbg0_mode7_config_t bad =
        {SAT_VDP2_RBG0_BITMAP_512x256, SAT_VDP2_COLOR_MODE_256,
         0x12000u, 0x10000u, 0u, 5u, 7u};
    OK(sat_vdp2_ground_environment_validate_layout(
        &ground, &bad, 224u, SAT_VDP2_VRAM_WORD_CAPACITY, 0u, 512u, 2u)
        == SAT_ERR_CAPACITY);
    std::puts("vdp2 environment layout: OK");
    return 0;
}
