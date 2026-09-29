#ifndef IKEMEN_FIGHT_H
#define IKEMEN_FIGHT_H

#include <stdint.h>

#include "ikemen_anim.h"
#include "ikemen_cns.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IK_SCREEN_W 320
#define IK_SCREEN_H 224
#define IK_FLOOR_Y 180
#define IK_STAGE_MIN_X 24
#define IK_STAGE_MAX_X 296
#define IK_MAX_HP 1000
#define IK_ROUND_TIME_FRAMES (99u * 60u)
#define IK_KO_FREEZE_FRAMES 120

typedef enum ik_state {
    IK_STATE_IDLE = 0,
    IK_STATE_WALK = 20,
    IK_STATE_JUMP = 40,
    IK_STATE_CROUCH = 50,
    IK_STATE_PUNCH = 200,
    IK_STATE_STRONG_PUNCH = 210,
    IK_STATE_KICK = 230,
    IK_STATE_STRONG_KICK = 240,
    IK_STATE_HIT = 500,
    IK_STATE_KO = 510,
    IK_STATE_GUARD = 550
} ik_state_t;

typedef enum ik_event {
    IK_EVENT_NONE = 0,
    IK_EVENT_HIT = 1u << 0,
    IK_EVENT_KO = 1u << 1,
    IK_EVENT_ROUND_OVER = 1u << 2,
    IK_EVENT_RESET = 1u << 3
} ik_event_t;

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
    int16_t state;
    uint16_t state_time;
    int16_t hp;
    uint16_t hitstun;
    uint16_t hit_pause;
    uint8_t attack_has_hit;
    uint8_t attack_id;
    uint8_t move_contact;
} ik_fighter_t;

typedef struct ik_fight {
    ik_fighter_t fighters[2];
    const ik_cns_asset_t* cns;
    uint32_t frame;
    uint32_t timer_frames;
    uint16_t events;
    uint8_t round_over;
    uint8_t winner;
    uint32_t hits_p1;
    uint32_t hits_p2;
    uint32_t ko_freeze;
} ik_fight_t;

void ik_fight_init(ik_fight_t* fight, const ik_cns_asset_t* cns);
void ik_fight_reset(ik_fight_t* fight);
int ik_action_for_state(const ik_cns_asset_t* cns, int16_t state);
void ik_fight_update(ik_fight_t* fight,
                     const ik_fight_controls_t* p1,
                     const ik_fight_controls_t* p2,
                     const ik_frame_table_t* frames);

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
