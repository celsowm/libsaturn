#ifndef IKEMEN_FIGHT_H
#define IKEMEN_FIGHT_H

#include <stdint.h>

#include "ikemen_anim.h"
#include "ikemen_cns.h"
#include "ikemen_entity.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IK_SCREEN_W 320
#define IK_SCREEN_H 224
#define IK_FLOOR_Y 180
/* Default player bounds with the camera centred: stage0 screenleft/right 15
 * around the 320 px view. The live bounds follow the camera (fighter
 * xmin_q8/xmax_q8). */
#define IK_STAGE_MIN_X 15
#define IK_STAGE_MAX_X 305
/* Stage x=0 sits at the middle of the 320 px view. */
#define IK_STAGE_CENTER_X 160
#define IK_MAX_HP 1000
#define IK_MAX_POWER 3000
#define IK_ROUND_TIME_FRAMES (99u * 60u)
/* Ticks between the intro ending (character leaves its intro state) and
 * RoundState 2, i.e. the "Round 1 / Fight" announcement. Measured from
 * upstream with the stock screenpack. */
#define IK_ROUND_FIGHT_WAIT_TICKS 111u
#define IK_KO_FREEZE_FRAMES 120
#define IK_MAX_EFFECT_EVENTS 2
#define IK_MAX_SOUND_EVENTS 4

typedef enum ik_state {
    IK_STATE_IDLE = 0,
    IK_STATE_CROUCH = 11,
    IK_STATE_WALK = 20,
    IK_STATE_JUMP = 40,

    IK_STATE_PUNCH = 200,
    IK_STATE_STRONG_PUNCH = 210,
    IK_STATE_KICK = 230,
    IK_STATE_STRONG_KICK = 240,

    IK_STATE_CROUCH_PUNCH = 400,
    IK_STATE_CROUCH_STRONG_PUNCH = 410,
    IK_STATE_CROUCH_KICK = 430,
    IK_STATE_CROUCH_STRONG_KICK = 440,

    IK_STATE_JUMP_PUNCH = 600,
    IK_STATE_JUMP_STRONG_PUNCH = 610,
    IK_STATE_JUMP_KICK = 630,
    IK_STATE_JUMP_STRONG_KICK = 640,

    IK_STATE_HIT = 500,
    IK_STATE_KO = 510,
    IK_STATE_GUARD = 550
} ik_state_t;

typedef enum ik_event {
    IK_EVENT_NONE = 0,
    IK_EVENT_HIT = 1u << 0,
    IK_EVENT_KO = 1u << 1,
    IK_EVENT_ROUND_OVER = 1u << 2,
    IK_EVENT_RESET = 1u << 3,
    IK_EVENT_GUARD = 1u << 4
} ik_event_t;

typedef struct ik_effect_event {
    int16_t action;
    int16_t x;
    int16_t y;
} ik_effect_event_t;

typedef struct ik_sound_event {
    int16_t group;
    int16_t item;
} ik_sound_event_t;

typedef struct ik_fight_controls {
    uint8_t forward;
    uint8_t back;
    uint8_t up;
    uint8_t down;
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t start;
    uint8_t recovery;
    int16_t requested_state;
    uint8_t has_state_request;
} ik_fight_controls_t;

/* Extra fractional byte under a Q8.8 value (a remainder, in 1/256 of the
 * Q8.8 step). It only counts while the Q8.8
 * value still equals q8_ref, so any other writer of the Q8.8 field (a VelSet,
 * a hit, a bind) silently drops the stale fraction. */
typedef struct ik_fine {
    int32_t q8_ref;
    int16_t lo;   /* signed: the Q8.8 part truncates toward zero */
} ik_fine_t;

typedef struct ik_fighter {
    int16_t x;
    int16_t y;
    int32_t x_q8;
    int32_t y_q8;
    int32_t vx_q8;
    int32_t vy_q8;

    int8_t facing;
    int8_t on_ground;
    int8_t ctrl;
    int8_t spr_priority;

    int16_t state;
    int16_t prev_state;
    uint16_t state_time;
    uint16_t state_entries;
    uint8_t anim_clock_pending;
    uint8_t statedef_pending;
    uint8_t in_guard_dist;
    uint8_t cur_state_type;
    uint8_t cur_move_type;
    int16_t anim;
    uint16_t anim_time;
    int8_t state_axis;
    uint8_t air_jumps_used;
    uint8_t up_latched;

    int16_t hp;
    /* Damage dealt this tick; upstream lowers life when the victim next
     * acts, one tick after the hit. */
    int16_t pending_damage;
    int16_t power;
    uint16_t hitstun;
    uint16_t hit_pause;
    uint16_t hit_shake_time;
    /* Set when a ChangeState leaves a get-hit flight state, whose VelAdd
     * already ran this tick. */
    uint8_t gravity_carry;
    int32_t gravity_carry_q16;   /* VelAdd the previous state already ran */
    /* Eight more fractional bits under the Q8.8 position and velocity,
     * valid while the Q8.8 value is unchanged (see ikf_fine_get). */
    ik_fine_t fine_x, fine_y, fine_vx, fine_vy;
    int32_t gethit_yaccel_q16;
    int16_t pending_power;
    uint16_t hit_slide_time;
    uint16_t hit_ctrl_time;
    int32_t gethit_vx_q8;
    int32_t gethit_vy_q8;
    int16_t gethit_yaccel_q8;
    uint8_t gethit_ground_type;
    uint8_t gethit_anim_type;
    uint8_t gethit_fall;
    int16_t gethit_fall_x_q8;
    int16_t gethit_fall_y_q8;
    uint8_t gethit_fall_x_set;
    uint8_t gethit_fall_recover;
    uint8_t gethit_fall_recover_time;
    int16_t gethit_fall_damage;
    uint16_t gethit_fall_envshake_time;
    int16_t gethit_fall_envshake_ampl;
    uint16_t gethit_fall_envshake_freq;
    uint16_t fall_time;
    int16_t juggle_points;
    uint8_t guard_type;

    int16_t push_back;
    int16_t push_front;
    /* Width controller edge widths (0 by default, reset every tick) and the
     * per-tick ScreenBound / camera tracking flags. */
    uint8_t body_air;           /* push widths were reset for an air state */
    int16_t edge_front;
    int16_t edge_back;
    uint8_t screen_bound;
    uint8_t move_camera_x;
    uint8_t move_camera_y;
    /* Offset a connecting HitDef's mindist/maxdist/snap asks for, applied
     * on the victim's next tick (upstream ghv.xoff / yoff). */
    int32_t snap_x_q8;
    int32_t snap_y_q8;
    uint8_t snap_flags;
    /* A reversed attacker keeps its juggle bookkeeping until it is hit
     * again (upstream hittmp = -1); the reverser keeps its target list for
     * two more ticks while its ReversalDef winds down. */
    uint8_t reversed;
    uint8_t frozen_tick;        /* this tick's controllers did not run (hit pause) */
    uint8_t drop_target_skip;
    int16_t body_height;

    uint32_t hitdef_hit_mask;
    uint8_t attack_id;
    uint8_t move_contact;
    uint8_t move_hit;
    uint16_t move_contact_time;
    uint8_t move_contact_type;   /* 0 hit, 1 guarded; survives state changes */
    uint16_t afterimage_time;
    uint8_t afterimage_length;
    uint8_t afterimage_timegap;
    uint8_t afterimage_framegap;
    uint32_t afterimage_bright_rgb;
    uint32_t afterimage_contrast_rgb;
    uint32_t afterimage_add_rgb;
    uint32_t afterimage_mul_rgb;
    uint16_t palfx_time;
    int16_t palfx_add_r;
    int16_t palfx_add_g;
    int16_t palfx_add_b;
    int16_t palfx_sin_r;
    int16_t palfx_sin_g;
    int16_t palfx_sin_b;
    uint16_t palfx_cycle;
    uint16_t palfx_phase;
    uint16_t palfx_mul_r;
    uint16_t palfx_mul_g;
    uint16_t palfx_mul_b;
    int16_t palfx_sinmul_r;
    int16_t palfx_sinmul_g;
    int16_t palfx_sinmul_b;
    uint16_t palfx_sinmul_cycle;
    uint16_t palfx_sinmul_phase;
    int8_t active_hitdef_local;
    int16_t active_hitdef_global;
    uint8_t active_hitdef_secondary;
    uint8_t pos_freeze_x;
    uint8_t pos_freeze_y;
    uint8_t pause_fired;
    uint64_t one_shot_controller_mask;
    uint8_t not_hit_by_mask;
    uint16_t not_hit_by_attr_mask;
    uint16_t not_hit_by_time;
    int8_t target_index;
    int8_t hitdef_target;
    int8_t juggle_owner;
    int32_t target_id;
    int8_t last_hit_owner;
    int32_t last_hit_id;
    int8_t bound_to;
    uint8_t bind_ticks;   /* TargetBind keeps the bind for one more tick */
    int32_t xmin_q8;      /* screen bound, x_q8 space (set by the camera) */
    int32_t xmax_q8;
    ik_entity_handle_t bound_entity;
    uint8_t owner_player;
    uint8_t state_owner;
    uint8_t anim_owner;
} ik_fighter_t;

/* Stage [Camera]/[PlayerInfo]/[Bound] values the fight needs, in stage
 * pixels (0 = stage centre). Defaults are Ikemen's stage0. */
typedef struct ik_stage_params {
    int16_t bound_left, bound_right;     /* camera */
    int16_t tension;
    int16_t screen_left, screen_right;   /* [Bound] */
    int16_t left_bound, right_bound;     /* leftbound/rightbound */
    int16_t p1_start_x, p2_start_x;
} ik_stage_params_t;

typedef struct ik_fight {
    ik_fighter_t fighters[2];
    const ik_cns_asset_t* cns;
    const ik_cns_asset_t* player_cns[2];
    const ik_frame_table_t* player_frames[2];
    ik_entity_pool_t* entities;
    ik_entity_handle_t player_entities[2];
    uint32_t frame;
    uint32_t timer_frames;
    uint16_t events;
    uint8_t round_over;
    uint8_t winner;
    uint32_t hits_p1;
    uint32_t hits_p2;
    uint32_t ko_freeze;
    uint16_t pause_time;
    uint16_t pause_move_time;
    uint16_t pause_end_cmd_buffer_time;
    int8_t pause_owner;
    uint8_t pause_is_super;
    uint16_t env_shake_time;
    int16_t env_shake_ampl;
    uint16_t env_shake_freq;
    uint16_t env_shake_phase;
    uint16_t super_darken_time;
    uint8_t round_state;
    uint8_t intro_asserted;
    uint16_t round_wait;
    ik_stage_params_t stage;
    uint8_t cam_skip_smoothing;
    int32_t cam_x_q16;     /* camera centre, stage px, Q16.16 */
    int32_t cam_half_q16;  /* half of the visible width (zoom-out widens it) */
    int32_t xmin_q8;
    int32_t xmax_q8;
    ik_effect_event_t effect_events[IK_MAX_EFFECT_EVENTS];
    uint8_t effect_count;
    ik_sound_event_t sound_events[IK_MAX_SOUND_EVENTS];
    uint8_t sound_count;
} ik_fight_t;

void ik_fight_init(ik_fight_t* fight, const ik_cns_asset_t* cns);
void ik_fight_init_players(
    ik_fight_t* fight,
    const ik_cns_asset_t* p1_cns,
    const ik_cns_asset_t* p2_cns);
void ik_fight_set_player_cns(
    ik_fight_t* fight, uint8_t player, const ik_cns_asset_t* cns);
void ik_fight_bind_entities(
    ik_fight_t* fight,
    ik_entity_pool_t* pool,
    ik_entity_handle_t p1,
    ik_entity_handle_t p2);
void ik_fight_reset(ik_fight_t* fight);
void ik_fight_set_stage(ik_fight_t* fight, const ik_stage_params_t* stage);
/* Camera centre in fighter x space (160 = stage centre), Q8.8. */
int32_t ik_fight_camera_x_q8(const ik_fight_t* fight);
int ik_action_for_state(const ik_cns_asset_t* cns, int16_t state);
uint8_t ik_fight_state_type(const ik_fight_t* fight,
                            const ik_fighter_t* fighter);
void ik_fight_update(ik_fight_t* fight,
                     const ik_fight_controls_t* p1,
                     const ik_fight_controls_t* p2,
                     const ik_frame_table_t* p1_frames,
                     const ik_frame_table_t* p2_frames);

int ik_fight_max_hp(const ik_fight_t* fight);
int ik_fight_max_hp_player(const ik_fight_t* fight, uint8_t player);
int ik_body_half_w(const ik_fighter_t* f);
int ik_body_h(const ik_fighter_t* f);
void ik_body_box(const ik_fighter_t* f, int* out_left, int* out_top,
                 int* out_right, int* out_bottom);
int ik_boxes_overlap(int l0, int t0, int r0, int b0,
                     int l1, int t1, int r1, int b1);
const char* ik_fight_status_text(const ik_fight_t* fight);
int ik_fight_status_needs_start(const ik_fight_t* fight);

#ifdef __cplusplus
}
#endif

#endif
