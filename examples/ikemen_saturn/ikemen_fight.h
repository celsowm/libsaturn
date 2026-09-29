#ifndef IKEMEN_FIGHT_H
#define IKEMEN_FIGHT_H

/* Shared Ikemen-Saturn subset simulation (KFM-style + training).
 *
 * Pure integer logic, no hardware access: both the Saturn example and the
 * host test include this. Positions are screen pixels (320x224 space).
 * Touching edges do NOT overlap (matches sat_box2_overlap semantics).
 */

#include <stdint.h>

#include "saturn/input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IK_SCREEN_W 320
#define IK_SCREEN_H 224
#define IK_FLOOR_Y 180
#define IK_STAGE_MIN_X 24
#define IK_STAGE_MAX_X 296
#define IK_WALK_SPEED 2
#define IK_JUMP_VELOCITY (-11)
#define IK_GRAVITY 1
#define IK_MAX_HP 1000
#define IK_ROUND_TIME_FRAMES (99u * 60u)
#define IK_PUNCH_RANGE 30
#define IK_PUNCH_DAMAGE 60
#define IK_KICK_RANGE 36
#define IK_KICK_DAMAGE 90
#define IK_ATTACK_ACTIVE_FRAMES 6
#define IK_HITSTUN_FRAMES 18
#define IK_KO_FREEZE_FRAMES 120

typedef enum ik_state {
    IK_STATE_IDLE = 0,
    IK_STATE_WALK = 20,
    IK_STATE_JUMP = 40,
    IK_STATE_CROUCH = 50,
    IK_STATE_PUNCH = 200,
    IK_STATE_KICK = 210,
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

typedef struct ik_fighter {
    int16_t x;
    int16_t y; /* feet position */
    int16_t vx;
    int16_t vy;
    int8_t facing; /* +1 right, -1 left */
    int8_t on_ground;
    int16_t state;
    uint16_t state_time;
    int16_t hp;
    uint16_t hitstun;
    uint8_t attack_has_hit;
    uint8_t attack_id;
} ik_fighter_t;

typedef struct ik_fight {
    ik_fighter_t fighters[2];
    uint32_t frame;
    uint32_t timer_frames;
    uint16_t events;
    uint8_t round_over;
    uint8_t winner; /* 0 none, 1 p1, 2 p2 */
    uint32_t hits_p1;
    uint32_t hits_p2;
    uint32_t ko_freeze;
} ik_fight_t;

void ik_fight_init(ik_fight_t* fight);
void ik_fight_reset(ik_fight_t* fight);
/* p2_pad may be NULL -> training dummy (stands guard every 4th second). */
void ik_fight_update(ik_fight_t* fight, const sat_pad_state_t* p1_pad,
                     const sat_pad_state_t* p2_pad);

int ik_body_half_w(const ik_fighter_t* f);
int ik_body_h(const ik_fighter_t* f);
void ik_body_box(const ik_fighter_t* f, int* out_left, int* out_top,
                 int* out_right, int* out_bottom);
int ik_attack_box(const ik_fighter_t* f, int* out_left, int* out_top,
                  int* out_right, int* out_bottom);
int ik_boxes_overlap(int l0, int t0, int r0, int b0,
                     int l1, int t1, int r1, int b1);
const char* ik_fight_status_text(const ik_fight_t* fight);
int ik_fight_status_needs_start(const ik_fight_t* fight);

#ifdef __cplusplus
}
#endif

#endif /* IKEMEN_FIGHT_H */
