#include "saturn/vdp2_environment.h"

namespace {
bool ranges_overlap(uint32_t a, uint32_t aw, uint32_t b, uint32_t bw) {
    const uint64_t ae = static_cast<uint64_t>(a) + aw;
    const uint64_t be = static_cast<uint64_t>(b) + bw;
    return static_cast<uint64_t>(a) < be && static_cast<uint64_t>(b) < ae;
}
uint32_t bitmap_words(sat_vdp2_rbg0_bitmap_size_t size) {
    return size == SAT_VDP2_RBG0_BITMAP_512x256 ? 65536u
        : (size == SAT_VDP2_RBG0_BITMAP_512x512 ? 131072u : 0u);
}
}

extern "C" sat_result_t sat_vdp2_ground_environment_validate_layout(
    const sat_vdp2_rbg0_ground_config_t* ground,
    const sat_vdp2_rbg0_mode7_config_t* mode7,
    uint16_t coefficient_height, uint32_t vram_words,
    uint32_t cram_offset, uint32_t cram_words,
    uint8_t nbg0_priority) {
    if (!ground || !mode7 || !coefficient_height ||
        vram_words > SAT_VDP2_VRAM_WORD_CAPACITY ||
        cram_offset + cram_words > SAT_VDP2_CRAM_WORD_CAPACITY ||
        nbg0_priority > 7u || mode7->rbg0_priority > 7u ||
        mode7->sprite_priority > 7u) return SAT_ERR_INVALID_ARG;
    const uint32_t bitmap = bitmap_words(mode7->bitmap_size);
    const uint32_t coeff = static_cast<uint32_t>(coefficient_height) * 2u;
    const uint32_t rot = 48u;
    if (!bitmap || mode7->bitmap_base_word + bitmap > vram_words ||
        mode7->rot_param_base_word + rot > vram_words ||
        ground->coef_base_word + coeff > vram_words ||
        ranges_overlap(mode7->bitmap_base_word, bitmap,
            mode7->rot_param_base_word, rot) ||
        ranges_overlap(mode7->bitmap_base_word, bitmap,
            ground->coef_base_word, coeff) ||
        ranges_overlap(mode7->rot_param_base_word, rot,
            ground->coef_base_word, coeff)) return SAT_ERR_CAPACITY;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_ground_environment_requirements(
    uint16_t height, uint32_t* coefficient_words) {
    if (!height || !coefficient_words) return SAT_ERR_INVALID_ARG;
    *coefficient_words = static_cast<uint32_t>(height) * 2u;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_ground_environment_init(
    sat_vdp2_ground_environment_t* environment,
    const sat_vdp2_rbg0_ground_config_t* ground,
    const sat_vdp2_rbg0_mode7_config_t* mode7,
    uint16_t* coefficient_words, uint32_t coefficient_capacity,
    uint16_t params[48], uint16_t height) {
    uint32_t required;
    if (!environment || !ground || !mode7 || !coefficient_words || !params ||
        !height || sat_vdp2_ground_environment_requirements(height, &required) != SAT_OK ||
        coefficient_capacity < required)
        return SAT_ERR_INVALID_ARG;
    *environment = {};
    environment->ground = *ground;
    environment->mode7 = *mode7;
    environment->coefficient_words = coefficient_words;
    environment->coefficient_capacity = coefficient_capacity;
    environment->params = params;
    environment->height = height;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_ground_environment_upload_coefficients(
    sat_vdp2_ground_environment_t* environment) {
    if (!environment || !environment->coefficient_words || !environment->height)
        return SAT_ERR_INVALID_ARG;
    for (uint16_t y = 0; y < environment->height; ++y) {
        sat_vdp2_rbg0_ground_encode_coefficient(&environment->ground, y,
            &environment->coefficient_words[y * 2u],
            &environment->coefficient_words[y * 2u + 1u]);
    }
    const sat_result_t st = sat_vdp2_vram_write_words(
        environment->ground.coef_base_word, environment->coefficient_words,
        static_cast<uint32_t>(environment->height) * 2u);
    if (st != SAT_OK) return st;
    const sat_result_t mode7 = sat_vdp2_rbg0_mode7_init(&environment->mode7);
    if (mode7 != SAT_OK) return mode7;
    environment->initialized = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_ground_environment_build_params(
    sat_vdp2_ground_environment_t* environment, int32_t camera_x, int32_t camera_y) {
    if (!environment || !environment->params || !environment->height)
        return SAT_ERR_INVALID_ARG;
    sat_vdp2_rbg0_ground_build_params(&environment->ground, camera_x, camera_y,
        environment->params);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_ground_environment_commit_params(
    sat_vdp2_ground_environment_t* environment) {
    if (!environment || !environment->params || !environment->initialized)
        return SAT_ERR_INVALID_ARG;
    return sat_vdp2_vram_write_words(environment->mode7.rot_param_base_word,
        environment->params, 48u);
}

extern "C" sat_result_t sat_vdp2_ground_environment_update(
    sat_vdp2_ground_environment_t* environment, int32_t camera_x, int32_t camera_y) {
    const sat_result_t st = sat_vdp2_ground_environment_build_params(
        environment, camera_x, camera_y);
    return st == SAT_OK ? sat_vdp2_ground_environment_commit_params(environment) : st;
}

extern "C" const uint16_t* sat_vdp2_ground_environment_params(
    const sat_vdp2_ground_environment_t* environment) {
    return environment ? environment->params : nullptr;
}
