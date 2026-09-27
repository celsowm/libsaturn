#include <cstdio>

#include "saturn/vdp2_environment.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

extern "C" sat_result_t sat_vdp2_vram_write_words(uint32_t, const uint16_t*, uint32_t) {
    return SAT_OK;
}
extern "C" sat_result_t sat_vdp2_rbg0_mode7_init(
    const sat_vdp2_rbg0_mode7_config_t*) { return SAT_OK; }
static uint16_t g_ktaof = 0xFFFFu;
extern "C" sat_result_t sat_vdp2_rbg0_set_ktaof(uint16_t ktaof) {
    g_ktaof = ktaof;
    return SAT_OK;
}

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
    /* Coefficient table address: KAst carries the low 16 bits of byte / 4,
     * KTAOF the rest. A1 needs no offset; B0 (behind a 512x512 bitmap) does. */
    OK(sat_vdp2_rbg0_ground_kast_word(0x12000u) == 0x9000u && sat_vdp2_rbg0_ground_ktaof(0x12000u) == 0u);
    OK(sat_vdp2_rbg0_ground_kast_word(0x22000u) == 0x1000u && sat_vdp2_rbg0_ground_ktaof(0x22000u) == 1u);
    OK(sat_vdp2_rbg0_ground_ktaof(0x3F000u) == 1u);
    {
        const sat_vdp2_rbg0_ground_config_t tall =
            {512u, 512u, 160u, 112u, 2u, 0u, 215u, 0x22000u};
        const sat_vdp2_rbg0_mode7_config_t mode512 =
            {SAT_VDP2_RBG0_BITMAP_512x512, SAT_VDP2_COLOR_MODE_256,
             0u, 0x20000u, 0u, 5u, 7u};
        OK(sat_vdp2_ground_environment_validate_layout(
            &tall, &mode512, 224u, SAT_VDP2_VRAM_WORD_CAPACITY, 0u, 512u, 2u) == SAT_OK);
        static uint16_t coefficients[224u * 2u];
        static uint16_t params[48];
        sat_vdp2_ground_environment_t env;
        OK(sat_vdp2_ground_environment_init(&env, &tall, &mode512, coefficients,
                                            224u * 2u, params, 224u) == SAT_OK);
        OK(sat_vdp2_ground_environment_upload_coefficients(&env) == SAT_OK);
        OK(g_ktaof == 1u);
        /* The same table inside A1 goes back to offset 0. */
        OK(sat_vdp2_ground_environment_init(&env, &ground, &mode, coefficients,
                                            224u * 2u, params, 224u) == SAT_OK);
        OK(sat_vdp2_ground_environment_upload_coefficients(&env) == SAT_OK);
        OK(g_ktaof == 0u);
    }
    std::puts("vdp2 environment layout: OK");
    return 0;
}
