#ifndef SATURN_VDP2_ENVIRONMENT_H
#define SATURN_VDP2_ENVIRONMENT_H

#include <stdint.h>
#include "saturn/vdp2.h"
#include "saturn/vdp2_rbg0_ground.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Caller-owned, bounded Mode-7 ground presenter. It owns the common
 * coefficient/rotation-table lifecycle; an example still owns its bitmap,
 * palettes and any deliberately custom matrix edits. */
typedef struct sat_vdp2_ground_environment {
    sat_vdp2_rbg0_ground_config_t ground;
    sat_vdp2_rbg0_mode7_config_t mode7;
    uint16_t* coefficient_words;
    uint32_t coefficient_capacity;
    uint16_t* params;
    uint16_t height;
    uint8_t initialized;
} sat_vdp2_ground_environment_t;

#define SAT_VDP2_VRAM_WORD_CAPACITY 262144u
#define SAT_VDP2_CRAM_WORD_CAPACITY 2048u

/* Validates the complete bounded layout before any register or VRAM write:
 * bitmap, rotation parameters, coefficient table, CRAM palette range and
 * layer priorities must fit and must not overlap. */
sat_result_t sat_vdp2_ground_environment_validate_layout(
    const sat_vdp2_rbg0_ground_config_t* ground,
    const sat_vdp2_rbg0_mode7_config_t* mode7,
    uint16_t coefficient_height, uint32_t vram_words,
    uint32_t cram_offset, uint32_t cram_words,
    uint8_t nbg0_priority);

sat_result_t sat_vdp2_ground_environment_requirements(
    uint16_t height, uint32_t* coefficient_words);
sat_result_t sat_vdp2_ground_environment_init(
    sat_vdp2_ground_environment_t* environment,
    const sat_vdp2_rbg0_ground_config_t* ground,
    const sat_vdp2_rbg0_mode7_config_t* mode7,
    uint16_t* coefficient_words, uint32_t coefficient_capacity,
    uint16_t params[48], uint16_t height);
sat_result_t sat_vdp2_ground_environment_upload_coefficients(
    sat_vdp2_ground_environment_t* environment);
sat_result_t sat_vdp2_ground_environment_build_params(
    sat_vdp2_ground_environment_t* environment,
    int32_t camera_x, int32_t camera_y);
sat_result_t sat_vdp2_ground_environment_commit_params(
    sat_vdp2_ground_environment_t* environment);
sat_result_t sat_vdp2_ground_environment_update(
    sat_vdp2_ground_environment_t* environment,
    int32_t camera_x, int32_t camera_y);
const uint16_t* sat_vdp2_ground_environment_params(
    const sat_vdp2_ground_environment_t* environment);

#ifdef __cplusplus
}
#endif
#endif
