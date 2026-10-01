#ifndef IKEMEN_ENTITY_RUNTIME_H
#define IKEMEN_ENTITY_RUNTIME_H

#include <stdint.h>

#include "ikemen_anim.h"
#include "ikemen_cns.h"
#include "ikemen_entity.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ik_entity_runtime;
typedef int (*ik_entity_target_controller_fn)(
    void* user,
    struct ik_entity_runtime* runtime,
    ik_entity_handle_t source,
    const ik_cns_controller_t* controller);

typedef struct ik_entity_runtime {
    ik_entity_pool_t* pool;
    const ik_cns_asset_t* cns;
    const ik_frame_table_t* frames[2];
    uint16_t command_masks[2];
    int16_t stage_min_x;
    int16_t stage_max_x;
    void* external_user;
    ik_entity_target_controller_fn target_controller;
} ik_entity_runtime_t;

void ik_entity_runtime_init(
    ik_entity_runtime_t* runtime,
    ik_entity_pool_t* pool,
    const ik_cns_asset_t* cns,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames);

void ik_entity_runtime_set_target_controller(
    ik_entity_runtime_t* runtime,
    void* user,
    ik_entity_target_controller_fn callback);

void ik_entity_runtime_set_stage_bounds(
    ik_entity_runtime_t* runtime,
    int16_t stage_min_x,
    int16_t stage_max_x);

void ik_entity_runtime_set_command_mask(
    ik_entity_runtime_t* runtime,
    uint8_t player,
    uint16_t command_mask);

int ik_entity_runtime_enter_state(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t handle,
    int16_t state_no);

int ik_entity_runtime_spawn_helper(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t parent,
    const ik_cns_helper_t* helper,
    ik_entity_handle_t* out_handle);

int ik_entity_runtime_spawn_explod(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t parent,
    const ik_cns_explod_t* explod,
    ik_entity_handle_t* out_handle);

int ik_entity_runtime_spawn_projectile_spec(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t parent,
    const ik_cns_projectile_t* projectile,
    ik_entity_handle_t* out_handle);

int ik_entity_runtime_spawn_projectile(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t parent,
    int32_t id,
    int16_t state_no,
    int32_t pos_x_q8,
    int32_t pos_y_q8,
    int32_t vel_x_q8,
    int32_t vel_y_q8,
    ik_entity_handle_t* out_handle);

void ik_entity_runtime_step_paused(
    ik_entity_runtime_t* runtime,
    uint8_t owner_player,
    int super_pause);

void ik_entity_runtime_step(ik_entity_runtime_t* runtime);

#ifdef __cplusplus
}
#endif

#endif
