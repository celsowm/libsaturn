#ifndef IKEMEN_CNS_H
#define IKEMEN_CNS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IK_CNS_Q8_ONE 256

typedef enum ik_cns_state_type {
    IK_CNS_STATE_UNCHANGED = 0,
    IK_CNS_STATE_STAND,
    IK_CNS_STATE_CROUCH,
    IK_CNS_STATE_AIR,
    IK_CNS_STATE_LIEDOWN
} ik_cns_state_type_t;

typedef enum ik_cns_move_type {
    IK_CNS_MOVE_UNCHANGED = 0,
    IK_CNS_MOVE_IDLE,
    IK_CNS_MOVE_ATTACK,
    IK_CNS_MOVE_HIT
} ik_cns_move_type_t;

typedef enum ik_cns_physics {
    IK_CNS_PHYS_NONE = 0,
    IK_CNS_PHYS_STAND,
    IK_CNS_PHYS_CROUCH,
    IK_CNS_PHYS_AIR
} ik_cns_physics_t;

typedef enum ik_cns_trigger_kind {
    IK_CNS_TRIGGER_ALWAYS = 0,
    IK_CNS_TRIGGER_TIME_EQ,
    IK_CNS_TRIGGER_ANIM_ELEM_EQ,
    IK_CNS_TRIGGER_ANIM_END
} ik_cns_trigger_kind_t;

typedef enum ik_cns_ground_type {
    IK_CNS_GROUND_NORMAL = 0,
    IK_CNS_GROUND_HIGH,
    IK_CNS_GROUND_LOW,
    IK_CNS_GROUND_TRIP
} ik_cns_ground_type_t;

typedef enum ik_cns_controller_type {
    IK_CNS_CTRL_CHANGE_STATE = 1,
    IK_CNS_CTRL_CTRL_SET,
    IK_CNS_CTRL_POS_ADD,
    IK_CNS_CTRL_SPR_PRIORITY
} ik_cns_controller_type_t;

enum {
    IK_CNS_HITDEF_FALL = 1u << 0,
    IK_CNS_HITDEF_FORCE_NO_FALL = 1u << 1
};

enum {
    IK_CNS_CTRL_HAS_CTRL = 1u << 0
};

typedef struct ik_cns_constants {
    int16_t life;

    int16_t ground_back;
    int16_t ground_front;
    int16_t air_back;
    int16_t air_front;
    int16_t height;

    int16_t walk_fwd_q8;
    int16_t walk_back_q8;
    int16_t run_fwd_x_q8;
    int16_t run_fwd_y_q8;
    int16_t run_back_x_q8;
    int16_t run_back_y_q8;

    int16_t jump_neu_x_q8;
    int16_t jump_neu_y_q8;
    int16_t jump_back_q8;
    int16_t jump_fwd_q8;

    int16_t yaccel_q8;
    int16_t stand_friction_q8;
    int16_t crouch_friction_q8;
    int16_t stand_friction_threshold_q8;
    int16_t crouch_friction_threshold_q8;
} ik_cns_constants_t;

typedef struct ik_cns_state {
    int16_t number;
    int16_t anim;
    int16_t power_add;
    int16_t velset_x_q8;
    int16_t velset_y_q8;
    int8_t state_type;
    int8_t move_type;
    int8_t physics;
    int8_t ctrl;
    int8_t spr_priority;
    uint8_t has_velset;
    uint16_t hitdef_ofs;
    uint8_t hitdef_count;
    uint16_t playsnd_ofs;
    uint8_t playsnd_count;
    uint16_t controller_ofs;
    uint8_t controller_count;
} ik_cns_state_t;

typedef struct ik_cns_hitdef {
    int16_t state_number;
    uint8_t trigger_kind;
    int16_t trigger_value;

    int16_t damage;
    int16_t guard_damage;
    uint8_t priority;
    uint8_t pause_p1;
    uint8_t pause_p2;

    uint8_t ground_type;
    uint8_t ground_slide_time;
    uint8_t ground_hit_time;
    uint8_t air_hit_time;

    int16_t ground_velocity_x_q8;
    int16_t ground_velocity_y_q8;
    int16_t air_velocity_x_q8;
    int16_t air_velocity_y_q8;

    int16_t spark_no;
    int16_t spark_x;
    int16_t spark_y;
    int16_t hit_sound_group;
    int16_t hit_sound_item;
    int16_t guard_sound_group;
    int16_t guard_sound_item;
    uint8_t flags;
} ik_cns_hitdef_t;

typedef struct ik_cns_playsnd {
    int16_t state_number;
    uint8_t trigger_kind;
    int16_t trigger_value;
    int16_t group;
    int16_t item;
} ik_cns_playsnd_t;

/* Compact controller record. Meaning of values:
 * ChangeState: value0=state, value1=ctrl when HAS_CTRL is set
 * CtrlSet:     value0=ctrl
 * PosAdd:      value0=x Q8.8, value1=y Q8.8
 * SprPriority: value0=priority
 */
typedef struct ik_cns_controller {
    int16_t state_number;
    uint8_t type;
    uint8_t trigger_kind;
    int16_t trigger_value;
    int16_t value0;
    int16_t value1;
    uint8_t flags;
} ik_cns_controller_t;

typedef struct ik_cns_asset {
    ik_cns_constants_t constants;
    const ik_cns_state_t* states;
    uint16_t state_count;
    const ik_cns_hitdef_t* hitdefs;
    uint16_t hitdef_count;
    const ik_cns_playsnd_t* playsnds;
    uint16_t playsnd_count;
    const ik_cns_controller_t* controllers;
    uint16_t controller_count;
} ik_cns_asset_t;

int16_t ik_cns_q8_from_int(int16_t value);
int16_t ik_cns_q8_to_int(int32_t value);
const ik_cns_state_t* ik_cns_find_state(const ik_cns_asset_t* asset,
                                        int16_t state_number);

int ik_cns_trigger_now(uint8_t trigger_kind, int16_t trigger_value,
                       uint16_t state_time, uint16_t anim_element,
                       uint16_t anim_element_time, int anim_ended);

const ik_cns_hitdef_t* ik_cns_active_hitdef(const ik_cns_asset_t* asset,
                                            int16_t state_number,
                                            uint16_t state_time,
                                            uint16_t anim_element);

#ifdef __cplusplus
}
#endif

#endif
