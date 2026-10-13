#ifndef LIBSATURN_CHARACTER_COMBAT_H
#define LIBSATURN_CHARACTER_COMBAT_H
#include "saturn/input.h"
#include "saturn/math3d.h"
enum { COMBAT_TICK_HZ=60, COMBAT_ROUND_SECONDS=60, COMBAT_HEALTH=100,
       COMBAT_ATTACK_FRAME=10, COMBAT_PUNCH_TICKS=28, COMBAT_KICK_TICKS=38,
       COMBAT_HIT_TICKS=20, COMBAT_KNOCKDOWN_TICKS=60, COMBAT_JUMP_TICKS=48,
       COMBAT_PUNCH_DAMAGE=8, COMBAT_KICK_DAMAGE=14, COMBAT_GUARD_DAMAGE=1,
       COMBAT_ATTACK_RANGE_Q16=SAT_FX16_ONE*6/5, COMBAT_BODY_RADIUS_Q16=SAT_FX16_ONE*3/10,
       COMBAT_BODY_DIAMETER_Q16=COMBAT_BODY_RADIUS_Q16*2,
       COMBAT_COLLISION_PASSES=4, COMBAT_COLLISION_SLOP_Q16=4,
       COMBAT_WALK_UNITS_PER_SECOND=2,
       COMBAT_AI_CYCLE_TICKS=COMBAT_TICK_HZ*2,
       COMBAT_AI_ATTACK_WINDOW_TICKS=COMBAT_TICK_HZ/3,
       COMBAT_STEP_Q16=COMBAT_WALK_UNITS_PER_SECOND*SAT_FX16_ONE/COMBAT_TICK_HZ,
       COMBAT_ARENA_HALF_Q16=SAT_FX16_ONE*3 };
typedef enum { COMBAT_IDLE, COMBAT_WALK, COMBAT_RUN, COMBAT_JUMP, COMBAT_PUNCH,
               COMBAT_KICK, COMBAT_GUARD, COMBAT_HIT, COMBAT_KNOCKDOWN } combat_state_t;
typedef struct {
    sat_vec3_t position;
    combat_state_t state;
    unsigned state_ticks, health, hit_serial, attack_serial, consecutive_hits;
    int facing;
} combat_fighter_t;
typedef struct {
    combat_fighter_t fighters[2];
    unsigned ticks, round_ticks, round_over, hits, blocked, knockdowns;
} combat_round_t;
void combat_reset(combat_round_t* round);
void combat_tick(combat_round_t* round, uint16_t player_buttons);
#endif
