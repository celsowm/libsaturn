#ifndef BH_SIM_H
#define BH_SIM_H

/* Bullet-hell game simulation: no hardware, no library state, deterministic.
 *
 * The playfield is logical 224 x 448 (TATE, upright). Positions are int32 in
 * 1/256 pixel (BH_FP), so (x << 8) is already the 16.16 world pixel that
 * saturn/collide2d.h and saturn/spatial.h take. Every pool is fixed size and
 * dense: removing an entry swaps the last one into its slot. The same seed and
 * input stream always produce the same state; bh_sim_hash() proves it.
 *
 * Collision:
 *   - enemy bullets vs the player's small hitbox is the hot path (up to 1024
 *     tests a frame), so it is an inline int32 test, equal to
 *     sat_circle_overlap at 16.16 including strict edges (host-tested);
 *   - player shots vs enemies use sat_spatial as the broad phase and
 *     sat_circle_overlap as the narrow phase. */

#include <stdint.h>

#include "saturn/spatial.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BH_FIELD_W 224
#define BH_FIELD_H 448
#define BH_FP_SHIFT 8
#define BH_FP(px) ((int32_t)(px) * 256)

#define BH_MAX_BULLETS 1024
#define BH_MAX_SHOTS 96
#define BH_MAX_ENEMIES 24
#define BH_MAX_EXPLOSIONS 40
#define BH_MAX_PICKUPS 16

#define BH_STAGES 3
#define BH_SHAPES 6   /* diamond orb ring star needle shard */
#define BH_COLORS 6   /* blue green orange pink red violet */
#define BH_EXPLOSION_FRAMES 8
#define BH_BANK_FRAMES 5 /* left, centre-left, centre, centre-right, right */

enum { BH_SHAPE_DIAMOND, BH_SHAPE_ORB, BH_SHAPE_RING, BH_SHAPE_STAR, BH_SHAPE_NEEDLE, BH_SHAPE_SHARD };
enum { BH_COLOR_BLUE, BH_COLOR_GREEN, BH_COLOR_ORANGE, BH_COLOR_PINK, BH_COLOR_RED, BH_COLOR_VIOLET };
enum { BH_ENEMY_DRONE, BH_ENEMY_INTERCEPTOR, BH_ENEMY_TURRET, BH_ENEMY_SCOUT, BH_ENEMY_BOSS, BH_ENEMY_TYPES };
enum { BH_PAT_AIMED, BH_PAT_FAN, BH_PAT_RING, BH_PAT_SPIRAL, BH_PAT_RAIN, BH_PATTERNS };
enum { BH_PICKUP_POWER, BH_PICKUP_BOMB, BH_PICKUP_SCORE, BH_PICKUP_TYPES };

#define BH_IN_UP 0x0001u
#define BH_IN_DOWN 0x0002u
#define BH_IN_LEFT 0x0004u
#define BH_IN_RIGHT 0x0008u
#define BH_IN_SHOT 0x0010u
#define BH_IN_BOMB 0x0020u
#define BH_IN_FOCUS 0x0040u

typedef struct bh_params {
    /* Hitbox radii in pixels, one per sprite family. The example overwrites
     * these from the atlas (collision_suggested_radius). */
    uint8_t bullet_radius[BH_SHAPES];
    uint8_t enemy_radius[BH_ENEMY_TYPES];
    uint8_t shot_radius[2];     /* player shot, option laser */
    uint8_t player_radius;      /* the tiny DoDonPachi-style core */
    uint8_t pickup_radius;
    uint16_t stage_frames;      /* the stage ends at this frame at the latest */
    uint16_t boss_frame;        /* the boss enters here */
} bh_params_t;

typedef struct bh_bullet { int32_t x, y; int16_t vx, vy; uint8_t shape, color; uint16_t age; } bh_bullet_t;
typedef struct bh_shot { int32_t x, y; int16_t vx, vy; uint8_t kind, damage, pad0, pad1; } bh_shot_t;
typedef struct bh_enemy {
    int32_t x, y, anchor_x;
    int16_t vx, vy, hp, aux;
    uint16_t age, fire;
    uint8_t type, color, pattern, flags;
} bh_enemy_t;
typedef struct bh_explosion { int32_t x, y; uint8_t frame, tick, delay, pad; } bh_explosion_t;
typedef struct bh_pickup { int32_t x, y; int16_t vy, pad; uint8_t type, color; uint16_t age; } bh_pickup_t;

typedef struct bh_player {
    int32_t x, y;
    uint8_t alive, bank, lives, bombs;
    uint8_t power;           /* 0..4: shot width and option count */
    uint8_t shot_cooldown;
    uint16_t invuln;         /* frames left; render blinks on bit 1 */
    uint16_t respawn;        /* frames until the ship returns */
    int16_t steer;           /* -12..12 held-direction counter for the bank */
    uint8_t focus;
    uint8_t color;
} bh_player_t;

typedef struct bh_stats {
    uint32_t bullet_tests;       /* bullet-vs-player tests executed, cumulative */
    uint32_t player_hits;        /* overlaps found (absorbed in god mode) */
    uint32_t shot_hits;
    uint32_t bullets_spawned;
    uint32_t bullets_dropped;    /* spawn refused: pool full */
    uint32_t bullets_culled;     /* left the field */
    uint32_t enemies_killed;
    uint16_t peak_bullets;
    uint16_t spatial_overflow;   /* enemy that could not be inserted (must stay 0) */
} bh_stats_t;

typedef struct bh_sim {
    bh_params_t params;
    uint32_t rng;
    uint32_t frame;
    uint32_t score;
    uint32_t scroll_y;           /* background scroll, 1/256 px, grows */
    uint16_t stage;              /* 0..BH_STAGES-1, then wraps */
    uint16_t stage_serial;       /* counts every stage change; 0 until the first */
    uint16_t stage_timer;
    uint16_t stage_end_delay;    /* frames left after the boss falls */
    uint8_t boss_spawned, boss_dead, god, game_over;
    uint16_t steady_target;      /* 0 = off */
    uint16_t steady_seq;
    uint16_t game_over_timer;
    uint16_t prev_input;
    uint16_t bomb_timer;         /* frames the bomb flash lasts */
    uint16_t bullet_count, shot_count, enemy_count, explosion_count, pickup_count;
    uint8_t spatial_ready;
    uint8_t stage_changed;       /* set for the frame a stage change happened */
    bh_player_t player;
    bh_stats_t stats;
    bh_bullet_t bullets[BH_MAX_BULLETS];
    bh_shot_t shots[BH_MAX_SHOTS];
    bh_enemy_t enemies[BH_MAX_ENEMIES];
    bh_explosion_t explosions[BH_MAX_EXPLOSIONS];
    bh_pickup_t pickups[BH_MAX_PICKUPS];
    /* sat_spatial storage (7 x 14 cells of 32 px) */
    sat_spatial_t spatial;
    uint16_t sp_heads[7 * 14];
    sat_spatial_entry_t sp_entries[192];
    uint16_t sp_stamps[BH_MAX_ENEMIES];
    sat_box2_t sp_items[BH_MAX_ENEMIES];
} bh_sim_t;

typedef struct bh_hit_pair { uint16_t shot, enemy; } bh_hit_pair_t;

/* Fixed-point trig: angle 0..255 is a full turn, 0 = +x, 64 = +y (down). Q15. */
int32_t bh_sin(uint32_t angle);
int32_t bh_cos(uint32_t angle);
/* Angle of (dx, dy), 0..255, within one angle unit. */
uint8_t bh_atan2(int32_t dy, int32_t dx);

void bh_params_default(bh_params_t* params);
void bh_sim_init(bh_sim_t* sim, const bh_params_t* params, uint32_t seed);
/* Advances one frame. `input` is a mask of BH_IN_*. */
void bh_sim_update(bh_sim_t* sim, uint16_t input);

/* Keeps exactly `count` enemy bullets alive (0 = normal play, 1..1024). The
 * player cannot die, but every collision test still runs and is counted. The
 * pool is pre-filled so a benchmark starts in steady state. */
void bh_sim_set_steady_bullets(bh_sim_t* sim, uint16_t count);
void bh_sim_set_god(bh_sim_t* sim, uint8_t god);
/* Jumps the stage clock (harness / attract mode). */
void bh_sim_set_stage_time(bh_sim_t* sim, uint16_t frames);

/* A deterministic autopilot: sweeps the field, always shoots, steers away from
 * the nearest bullet. Used for the attract demo and the benchmark. */
uint16_t bh_demo_input(const bh_sim_t* sim);

uint32_t bh_sim_hash(const bh_sim_t* sim);

/* Non-mutating queries (the update uses the same code). */
uint16_t bh_bullets_hitting_player(const bh_sim_t* sim, uint16_t* first);
uint16_t bh_find_shot_hits(bh_sim_t* sim, bh_hit_pair_t* out, uint16_t cap);

/* Options: 0 below power 2, 1 at power 2, 2 from power 3. Slot i hangs beside
 * the ship (wider when not focused); position in 1/256 px. */
static inline int bh_option_count(const bh_player_t* p) { return p->power >= 3 ? 2 : p->power >= 2 ? 1 : 0; }
static inline void bh_option_pos(const bh_player_t* p, int i, int32_t* x, int32_t* y) {
    const int32_t side = (p->focus ? 14 : 24) * 256;
    *x = p->x + (i == 0 ? -side : side);
    *y = p->y + 6 * 256;
}

/* Circle test used for bullet-vs-player: |d| < r strictly, all in 1/256 px.
 * Equal to sat_circle_overlap on (x << 8) 16.16 centres and radii. */
static inline int bh_circle_hit(int32_t dx, int32_t dy, int32_t r) {
    if (dx >= r || dx <= -r || dy >= r || dy <= -r) return 0;
    return dx * dx + dy * dy < r * r;
}

#ifdef __cplusplus
}
#endif
#endif
