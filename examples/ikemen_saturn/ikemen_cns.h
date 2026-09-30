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
    IK_CNS_TRIGGER_ANIM_END,
    IK_CNS_TRIGGER_ANIM_ELEM_RANGE,
    IK_CNS_TRIGGER_MOVE_CONTACT_ELEM_WINDOW,
    IK_CNS_TRIGGER_COMMAND_ACTIVE,
    IK_CNS_TRIGGER_COMMAND_INACTIVE,
    IK_CNS_TRIGGER_ABS_VX_LT_Q8,
    IK_CNS_TRIGGER_VY_GT_Q8_AT_FLOOR,
    IK_CNS_TRIGGER_ANIM_EQ_AND_END,
    IK_CNS_TRIGGER_HIT_SLIDE_TIME,
    IK_CNS_TRIGGER_HIT_SLIDE_GE,
    IK_CNS_TRIGGER_HIT_CTRL_TIME,
    IK_CNS_TRIGGER_HIT_OVER,
    IK_CNS_TRIGGER_HIT_LAUNCH,
    IK_CNS_TRIGGER_HIT_NO_LAUNCH,
    IK_CNS_TRIGGER_NOT_ALIVE,
    IK_CNS_TRIGGER_ANIM_ELEM_BEFORE,
    IK_CNS_TRIGGER_STATE_AXIS_FWD_ANIM_ELEM_EQ,
    IK_CNS_TRIGGER_NOT_BOUND,
    IK_CNS_TRIGGER_THROW_GROUND_RECOVERY,
    IK_CNS_TRIGGER_THROW_AIR_RECOVERY,
    IK_CNS_TRIGGER_ANIM_ELEM_EQ_OR,
    IK_CNS_TRIGGER_HIT_SHAKE_OVER,
    IK_CNS_TRIGGER_AIR_NEAR_BODY_EDGE,
    IK_CNS_TRIGGER_STATE_ENTRY_FRONT_EDGE_BODY_LE,
    IK_CNS_TRIGGER_STATE_ENTRY_BACK_EDGE_LT,
    IK_CNS_TRIGGER_COMMAND_ANY_VY_LT_Q8,
    IK_CNS_TRIGGER_VY_GT_Q8_AT_LEVEL,
    IK_CNS_TRIGGER_VY_GE_Q8
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
    IK_CNS_CTRL_SPR_PRIORITY,
    IK_CNS_CTRL_CHANGE_ANIM,
    IK_CNS_CTRL_WIDTH,
    IK_CNS_CTRL_VEL_SET,
    IK_CNS_CTRL_VEL_MUL,
    IK_CNS_CTRL_POS_SET,
    IK_CNS_CTRL_CHANGE_ANIM_BY_VX,
    IK_CNS_CTRL_CHANGE_ANIM_IF_END_FROM,
    IK_CNS_CTRL_CAPTURE_COMMAND_AXIS,
    IK_CNS_CTRL_JUMP_LAUNCH,
    IK_CNS_CTRL_AIR_JUMP_LAUNCH,
    IK_CNS_CTRL_CHANGE_ANIM_IF_EXISTS,
    IK_CNS_CTRL_CHANGE_ANIM_DESCENT_IF_EXISTS,
    IK_CNS_CTRL_GUARD_ANIM_BY_TYPE,
    IK_CNS_CTRL_GUARD_STATE_BY_TYPE,
    IK_CNS_CTRL_GUARD_END,
    IK_CNS_CTRL_HIT_VEL_SET,
    IK_CNS_CTRL_GET_HIT_ANIM,
    IK_CNS_CTRL_HIT_RECOVER_STATE,
    IK_CNS_CTRL_FALL_BOUNCE_VEL,
    IK_CNS_CTRL_FALL_GROUND_BRANCH,
    IK_CNS_CTRL_POS_ADD_VEL,
    IK_CNS_CTRL_VEL_ADD,
    IK_CNS_CTRL_FALL_RECOVERY,
    IK_CNS_CTRL_DOWNED_HIT_BRANCH,
    IK_CNS_CTRL_TARGET_BIND,
    IK_CNS_CTRL_TARGET_FACING,
    IK_CNS_CTRL_TARGET_LIFE_ADD,
    IK_CNS_CTRL_TARGET_STATE,
    IK_CNS_CTRL_TURN,
    IK_CNS_CTRL_CHANGE_ANIM2,
    IK_CNS_CTRL_SELF_STATE,
    IK_CNS_CTRL_VEL_MUL_X_BY_ANIM_ELEM,
    IK_CNS_CTRL_POS_ADD_FROM_BACK_EDGE,
    IK_CNS_CTRL_POS_FREEZE,
    IK_CNS_CTRL_VAR_SET,
    IK_CNS_CTRL_VAR_ADD,
    IK_CNS_CTRL_HELPER,
    IK_CNS_CTRL_DESTROY_SELF
} ik_cns_controller_type_t;

typedef enum ik_cns_helper_postype {
    IK_CNS_HELPER_POS_P1 = 0,
    IK_CNS_HELPER_POS_P2
} ik_cns_helper_postype_t;

enum {
    IK_CNS_HITDEF_FALL = 1u << 0,
    IK_CNS_HITDEF_FORCE_NO_FALL = 1u << 1,
    IK_CNS_HITDEF_THROW = 1u << 2,
    IK_CNS_HITDEF_AIR_FALL = 1u << 3,
    IK_CNS_HITDEF_FORCE_STAND = 1u << 4
};

enum {
    IK_CNS_GUARD_STAND = 1u << 0,
    IK_CNS_GUARD_CROUCH = 1u << 1,
    IK_CNS_GUARD_AIR = 1u << 2
};

typedef enum ik_cns_priority_type {
    IK_CNS_PRIORITY_HIT = 0,
    IK_CNS_PRIORITY_MISS,
    IK_CNS_PRIORITY_DODGE
} ik_cns_priority_type_t;

typedef enum ik_cns_p2_dist_op {
    IK_CNS_P2_DIST_NONE = 0,
    IK_CNS_P2_DIST_LT,
    IK_CNS_P2_DIST_LE,
    IK_CNS_P2_DIST_GT,
    IK_CNS_P2_DIST_GE
} ik_cns_p2_dist_op_t;

enum {
    IK_CNS_HIT_STAND = 1u << 0,
    IK_CNS_HIT_CROUCH = 1u << 1,
    IK_CNS_HIT_AIR = 1u << 2,
    IK_CNS_HIT_FALL = 1u << 3,
    IK_CNS_HIT_DOWN = 1u << 4,
    IK_CNS_HIT_ONLY_GETHIT = 1u << 5,
    IK_CNS_HIT_NOT_GETHIT = 1u << 6,
    IK_CNS_HIT_DEFAULT =
        IK_CNS_HIT_STAND | IK_CNS_HIT_CROUCH |
        IK_CNS_HIT_AIR | IK_CNS_HIT_FALL
};

enum {
    IK_CNS_CTRL_HAS_CTRL = 1u << 0,
    IK_CNS_CTRL_IGNORE_HIT_PAUSE = 1u << 1,
    IK_CNS_CTRL_AXIS_X = 1u << 2,
    IK_CNS_CTRL_AXIS_Y = 1u << 3,
    IK_CNS_CTRL_LOCAL_X = 1u << 4
};

enum {
    IK_CNS_COMMAND_HOLD_FWD = 1u << 0,
    IK_CNS_COMMAND_HOLD_BACK = 1u << 1,
    IK_CNS_COMMAND_HOLD_UP = 1u << 2,
    IK_CNS_COMMAND_HOLD_DOWN = 1u << 3,
    IK_CNS_COMMAND_RECOVERY = 1u << 4,
    IK_CNS_COMMAND_A = 1u << 5,
    IK_CNS_COMMAND_B = 1u << 6
};

typedef struct ik_cns_constants {
    int16_t life;

    int16_t ground_back;
    int16_t ground_front;
    int16_t air_back;
    int16_t air_front;
    int16_t height;
    int16_t attack_dist;

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
    int16_t run_jump_fwd_x_q8;
    int16_t run_jump_fwd_y_q8;

    int16_t air_jump_neu_x_q8;
    int16_t air_jump_neu_y_q8;
    int16_t air_jump_back_q8;
    int16_t air_jump_fwd_q8;
    int16_t air_jump_num;
    int16_t air_jump_height;

    int16_t yaccel_q8;
    int16_t stand_friction_q8;
    int16_t crouch_friction_q8;
    int16_t stand_friction_threshold_q8;
    int16_t crouch_friction_threshold_q8;

    int16_t liedown_time;
    int16_t air_gethit_groundlevel_q8;
    int16_t air_gethit_trip_groundlevel_q8;
    int16_t down_bounce_offset_x_q8;
    int16_t down_bounce_offset_y_q8;
    int16_t down_bounce_yaccel_q8;
    int16_t down_bounce_groundlevel_q8;
    int16_t down_friction_threshold_q8;

    int16_t air_gethit_groundrecover_x_q8;
    int16_t air_gethit_groundrecover_y_q8;
    int16_t air_gethit_groundrecover_threshold_q8;
    int16_t air_gethit_groundrecover_groundlevel_q8;
    int16_t air_gethit_airrecover_mul_x_q8;
    int16_t air_gethit_airrecover_mul_y_q8;
    int16_t air_gethit_airrecover_add_x_q8;
    int16_t air_gethit_airrecover_add_y_q8;
    int16_t air_gethit_airrecover_back_q8;
    int16_t air_gethit_airrecover_fwd_q8;
    int16_t air_gethit_airrecover_up_q8;
    int16_t air_gethit_airrecover_down_q8;
    int16_t air_gethit_airrecover_threshold_q8;
    int16_t air_gethit_airrecover_yaccel_q8;
    int16_t air_juggle;
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
    int16_t land_state;
    int16_t air_accel_q8;
    int16_t land_level_q8;
    uint16_t air_motion_start;
    uint8_t land_ctrl;
    int16_t juggle;
    uint8_t has_juggle;
    uint8_t owns_air_accel;
    uint8_t hitdef_persist;
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

    uint8_t guard_flags;
    uint8_t guard_kill;
    uint8_t guard_slide_time;
    uint8_t guard_hit_time;
    uint8_t guard_ctrl_time;
    int16_t guard_velocity_x_q8;
    int16_t air_guard_velocity_x_q8;
    int16_t air_guard_velocity_y_q8;
    uint8_t anim_type;
    uint8_t air_anim_type;
    int16_t fall_x_velocity_q8;
    int16_t fall_y_velocity_q8;
    uint8_t fall_x_velocity_set;
    uint8_t fall_recover;
    uint8_t fall_recover_time;

    uint16_t down_hit_time;
    int16_t down_velocity_x_q8;
    int16_t down_velocity_y_q8;
    uint8_t down_bounce;
    uint8_t hit_flags;
    uint8_t priority_type;
    uint8_t air_juggle;

    int16_t p1_state_no;
    int16_t p2_state_no;
    int16_t guard_dist;
    int8_t p1_facing;
    int8_t p2_facing;
    int8_t p1_spr_priority;
    uint8_t p2_body_dist_op;
    int16_t p2_body_dist_x;
    int16_t alt_damage;
    int16_t alt_damage_prev_state;
    uint8_t trigger2_kind;
    int16_t trigger2_value;
    int16_t yaccel_q8;
    uint8_t has_trigger2;
    uint8_t has_alt_damage;
    int16_t ground_cornerpush_veloff_q8;
    int16_t trigger2_spark_y;
    int16_t guard_spark_no;
} ik_cns_hitdef_t;

typedef struct ik_cns_playsnd {
    int16_t state_number;
    uint8_t trigger_kind;
    int16_t trigger_value;
    int16_t group;
    int16_t item;
} ik_cns_playsnd_t;

/* Compact controller record.
 *
 * trigger_value / trigger_value2:
 *   simple triggers: primary value / 0
 *   ANIM_ELEM_RANGE: inclusive first element / exclusive last element
 *   MOVE_CONTACT_ELEM_WINDOW: first/last AnimElemTime element numbers
 *
 * value0 / value1:
 *   ChangeState: target state / ctrl
 *   CtrlSet: ctrl / 0
 *   PosAdd: x / y in Q8.8
 *   SprPriority: priority / 0
 *   ChangeAnim: action / 1-based element
 *   Width: front/back additions in pixels (MUGEN value shorthand)
 *   VelSet: x/y in Q8.8; AXIS_X/AXIS_Y select written axes
 *   VelMul: x/y multipliers in Q8.8; AXIS_X/AXIS_Y select axes
 *   PosSet: x/y in Q8.8 relative to the stage floor for y
 *   ChangeAnimByVx: neutral action (or -1) / forward action; back=forward+1
 *   ChangeAnimIfEndFrom: source action / destination action
 *   CaptureCommandAxis: remembers holdback/holdfwd in the current state
 *   JumpLaunch/AirJumpLaunch: use compiled Velocity constants and remembered axis
 *   ChangeAnimIfExists: preferred action / fallback action
 *   ChangeAnimDescentIfExists: Y threshold / first ascending action
 *   GuardAnimByType: base stand action; crouch=base+1, air=base+2
 *   GuardStateByType: base stand state; crouch=base+1, air=base+2
 *   GuardEnd: return to stand/crouch/air locomotion after guard end
 *   HitVelSet: stored get-hit X/Y velocity, selected by AXIS flags
 *   GetHitAnim: choose common get-hit animation from stored HitDef metadata
 *   HitRecoverState: branch to 5040/5050 according to fall state
 *   FallBounceVel: apply HitDef fall velocity for the ground bounce
 *   FallGroundBranch: skip bounce when HitDef fall.yvelocity is zero
 *   PosAddVel: integrate selected velocity axes for Physics=N states
 *   DownedHitBranch: select downed hit anim/state from stored get-hit Y velocity
 *   VelAdd: add Q8.8 velocity on selected axes
 *   FallRecovery: enter ground/air fall recovery using compiled thresholds
 */
typedef struct ik_cns_controller {
    int16_t state_number;
    uint8_t type;
    uint8_t trigger_kind;
    int16_t trigger_value;
    int16_t trigger_value2;
    int32_t value0;
    int32_t value1;
    uint8_t flags;
} ik_cns_controller_t;

typedef struct ik_cns_controller_context {
    uint16_t state_time;
    uint16_t anim_element;
    uint16_t anim_element_time;
    int16_t anim;
    int32_t vx_q8;
    int32_t vy_q8;
    int32_t y_q8;
    int32_t floor_y_q8;
    uint16_t command_mask;
    uint16_t hitstun;
    uint16_t hit_pause;
    uint16_t hit_slide_time;
    uint16_t hit_ctrl_time;
    uint16_t fall_time;
    int16_t back_edge_body_dist;
    int16_t front_edge_body_dist;
    int16_t back_edge_dist;
    int8_t state_axis;
    uint8_t hit_launch;
    uint8_t alive;
    uint8_t can_recover;
    uint8_t is_bound;
    uint8_t anim_ended;
    uint8_t move_contact;
} ik_cns_controller_context_t;

typedef struct ik_cns_helper {
    int32_t id;
    int16_t state_no;
    int32_t pos_x_q8;
    int32_t pos_y_q8;
    int8_t facing;
    uint8_t postype;
    uint8_t keyctrl;
    uint8_t ownpal;
} ik_cns_helper_t;

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
    const ik_cns_helper_t* helpers;
    uint16_t helper_count;
} ik_cns_asset_t;

int16_t ik_cns_q8_from_int(int16_t value);
int16_t ik_cns_q8_to_int(int32_t value);
const ik_cns_state_t* ik_cns_find_state(const ik_cns_asset_t* asset,
                                        int16_t state_number);

int ik_cns_trigger_now(uint8_t trigger_kind, int16_t trigger_value,
                       uint16_t state_time, uint16_t anim_element,
                       uint16_t anim_element_time, int anim_ended);

int ik_cns_controller_trigger_now(const ik_cns_controller_t* controller,
                                  uint16_t state_time,
                                  uint16_t anim_element,
                                  uint16_t anim_element_time,
                                  int anim_ended,
                                  int move_contact);
int ik_cns_controller_trigger_context_now(
    const ik_cns_controller_t* controller,
    const ik_cns_controller_context_t* context);

const ik_cns_hitdef_t* ik_cns_active_hitdef(const ik_cns_asset_t* asset,
                                            int16_t state_number,
                                            uint16_t state_time,
                                            uint16_t anim_element);

#ifdef __cplusplus
}
#endif

#endif
