#ifndef IKEMEN_ENTITY_H
#define IKEMEN_ENTITY_H

#include <stdint.h>

#include "ikemen_expr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IK_ENTITY_CAPACITY 16u
#define IK_ENTITY_VAR_COUNT 60u
#define IK_ENTITY_FVAR_COUNT 40u
#define IK_ENTITY_SYSVAR_COUNT 5u
#define IK_ENTITY_INVALID_SLOT 0xFFu
#define IK_ENTITY_Q8_ONE 256

typedef enum ik_entity_type {
    IK_ENTITY_NONE = 0,
    IK_ENTITY_PLAYER,
    IK_ENTITY_HELPER,
    IK_ENTITY_PROJECTILE,
    IK_ENTITY_EXPLOD
} ik_entity_type_t;

typedef struct ik_entity_handle {
    uint8_t slot;
    uint8_t generation;
} ik_entity_handle_t;

typedef struct ik_entity {
    uint8_t type;
    uint8_t owner_player;
    int32_t id;

    ik_entity_handle_t parent;
    ik_entity_handle_t root;
    ik_entity_handle_t target;

    int32_t x_q8;
    int32_t y_q8;
    int32_t vx_q8;
    int32_t vy_q8;
    int32_t ax_q8;
    int32_t ay_q8;
    int16_t remove_time;
    int16_t state_no;
    int16_t projectile_main_anim_no;
    int16_t projectile_hit_anim_no;
    int16_t projectile_remove_anim_no;
    int16_t projectile_cancel_anim_no;
    int16_t projectile_edge_bound;
    int16_t projectile_stage_bound;
    int16_t prev_state_no;
    int16_t anim_no;
    uint16_t state_time;
    uint16_t anim_time;
    int16_t life;
    int16_t power;
    uint16_t active_hit_attr_mask;
    int16_t push_back;
    int16_t push_front;
    int8_t facing;
    int8_t spr_priority;
    uint8_t ctrl;
    uint8_t state_type;
    uint8_t move_type;
    uint8_t move_contact;
    uint8_t keyctrl;
    uint8_t ownpal;
    uint8_t projectile_hits_left;
    uint8_t projectile_miss_time;
    uint8_t projectile_hit_cooldown;
    uint8_t projectile_priority;
    uint8_t projectile_remove_on_hit;
    int16_t projectile_velmul_x_q8;
    int16_t projectile_velmul_y_q8;
    uint8_t pause_move_time;
    uint8_t super_move_time;

    uint16_t hit_pause;
    uint32_t hitdef_hit_mask;
    int16_t active_hitdef_global;
    int8_t active_hitdef_local;
    uint8_t active_hitdef_secondary;

    int32_t vars[IK_ENTITY_VAR_COUNT];
    int32_t fvars_q16[IK_ENTITY_FVAR_COUNT];
    int32_t sysvars[IK_ENTITY_SYSVAR_COUNT];
} ik_entity_t;

typedef struct ik_entity_pool {
    ik_entity_t entities[IK_ENTITY_CAPACITY];
    uint8_t generations[IK_ENTITY_CAPACITY];
    ik_entity_handle_t players[2];
} ik_entity_pool_t;

typedef struct ik_entity_expr_binding {
    const ik_entity_pool_t* pool;
    ik_entity_handle_t self;
} ik_entity_expr_binding_t;

ik_entity_handle_t ik_entity_invalid_handle(void);
int ik_entity_handle_is_valid(ik_entity_handle_t handle);
int ik_entity_handle_equal(ik_entity_handle_t a, ik_entity_handle_t b);

void ik_entity_pool_init(ik_entity_pool_t* pool);

int ik_entity_spawn(
    ik_entity_pool_t* pool,
    uint8_t type,
    int32_t id,
    uint8_t owner_player,
    ik_entity_handle_t parent,
    ik_entity_handle_t* out_handle);

int ik_entity_destroy(ik_entity_pool_t* pool, ik_entity_handle_t handle);

ik_entity_t* ik_entity_get(
    ik_entity_pool_t* pool,
    ik_entity_handle_t handle);

const ik_entity_t* ik_entity_get_const(
    const ik_entity_pool_t* pool,
    ik_entity_handle_t handle);

int ik_entity_set_target(
    ik_entity_pool_t* pool,
    ik_entity_handle_t source,
    ik_entity_handle_t target);

ik_entity_handle_t ik_entity_redirect(
    const ik_entity_pool_t* pool,
    ik_entity_handle_t self,
    uint8_t redirect);

uint8_t ik_entity_count_type(
    const ik_entity_pool_t* pool,
    uint8_t type);

int ik_entity_expr_read_field(
    void* user,
    uint8_t redirect,
    uint8_t field,
    int16_t index,
    int32_t* out_value);

#ifdef __cplusplus
}
#endif

#endif
