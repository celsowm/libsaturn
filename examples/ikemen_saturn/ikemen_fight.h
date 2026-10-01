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
#define IK_STAGE_MIN_X 24
#define IK_STAGE_MAX_X 296
#define IK_MAX_HP 1000
#define IK_MAX_POWER 3000
#define IK_ROUND_TIME_FRAMES (99u * 60u)
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
    int16_t anim;
    uint16_t anim_time;
    int8_t state_axis;
    uint8_t air_jumps_used;
    uint8_t up_latched;

    int16_t hp;
    int16_t power;
    uint16_t hitstun;
    uint16_t hit_pause;
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
    int16_t body_height;

    uint32_t hitdef_hit_mask;
    uint8_t attack_id;
    uint8_t move_contact;
    uint8_t move_hit;
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
    int32_t target_id;
    int8_t last_hit_owner;
    int32_t last_hit_id;
    int8_t bound_to;
    ik_entity_handle_t bound_entity;
    uint8_t owner_player;
    uint8_t state_owner;
    uint8_t anim_owner;
} ik_fighter_t;

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
    ik_effect_event_t effect_events[IK_MAX_EFFECT_EVENTS];
    uint8_t effect_count;
    ik_sound_event_t sound_events[IK_MAX_SOUND_EVENTS];
    uint8_t sound_count;
} ik_fight_t;

void ik_fight_init(ik_fight_t* fight, const ik_cns_asset_t* cns);
void ik_fight_set_player_cns(
    ik_fight_t* fight, uint8_t player, const ik_cns_asset_t* cns);
void ik_fight_bind_entities(
    ik_fight_t* fight,
    ik_entity_pool_t* pool,
    ik_entity_handle_t p1,
    ik_entity_handle_t p2);
void ik_fight_reset(ik_fight_t* fight);
int ik_action_for_state(const ik_cns_asset_t* cns, int16_t state);
uint8_t ik_fight_state_type(const ik_fight_t* fight,
                            const ik_fighter_t* fighter);
void ik_fight_update(ik_fight_t* fight,
                     const ik_fight_controls_t* p1,
                     const ik_fight_controls_t* p2,
                     const ik_frame_table_t* p1_frames,
                     const ik_frame_table_t* p2_frames);

int ik_fight_max_hp(const ik_fight_t* fight);
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
