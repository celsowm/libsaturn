#ifndef HSP_GAME_H
#define HSP_GAME_H

/* The game of the high_speed_platformer example: no hardware in here. Everything it needs comes
 * from the public saturn/ modules and the generated stage data, so the same code runs in the
 * Saturn build and in the host test that plays the stage without a video chip. */

#include <stdint.h>

#include "saturn/character2.h"
#include "saturn/entity_stream2.h"
#include "saturn/follow_camera2d.h"
#include "saturn/input.h"
#include "saturn/path2.h"
#include "saturn/physics2_world.h"
#include "saturn/spatial.h"
#include "saturn/sprite_clip.h"
#include "saturn/terrain2.h"

#include "high_speed_platformer/layout.h"
#include "high_speed_platformer/stage.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HSP_VIEW_W 320
#define HSP_VIEW_H 224

#define HSP_MAX_OBJECTS 64u
#define HSP_NO_DESC 0xFFFFu
#define HSP_STREAM_WORDS 16u
#define HSP_COLLIDER_CAP 16u
#define HSP_HERO_HALF_W 6
#define HSP_HERO_HALF_H 12

/* collider categories */
#define HSP_CAT_PLAYER 0x0001u
#define HSP_CAT_PLATFORM 0x0002u

typedef enum hsp_object_kind {
    HSP_OBJ_FREE = 0,
    HSP_OBJ_RING,
    HSP_OBJ_SPARKLE,   /* a collected ring finishing its animation: no descriptor behind it */
    HSP_OBJ_DASH,
    HSP_OBJ_SPRING,
    HSP_OBJ_FLAG,      /* a checkpoint */
    HSP_OBJ_GOAL
} hsp_object_kind_t;

typedef struct hsp_object {
    uint8_t kind;
    uint8_t flags;
    uint16_t desc;       /* the descriptor it was made from, or HSP_NO_DESC */
    int16_t x, y;        /* world pixels; the clip's pivot sits here */
    uint16_t data;
    sat_clip_player_t clip;
} hsp_object_t;

typedef enum hsp_mode {
    HSP_MODE_RUN = 0,
    HSP_MODE_RAIL = 1,   /* hanging from the rail: a generic path follower */
    HSP_MODE_CLEAR = 2   /* the finish line was crossed */
} hsp_mode_t;

typedef struct hsp_platform_state {
    sat_collider2_t collider;
    sat_path2_t path;
    sat_fx16_t distance;
    int8_t direction;
    uint8_t reserved[3];
} hsp_platform_state_t;

/* Counters the host test asserts on and the HUD shows. */
typedef struct hsp_stats {
    uint32_t ticks;
    uint32_t rings;
    uint32_t deaths;
    uint32_t footsteps;       /* clip events of the run animation */
    uint32_t landings;
    uint32_t dashes;
    uint32_t springs;
    uint32_t rail_grabs;
    uint32_t layer_switches;
    uint32_t platform_ticks;  /* ticks spent standing on a moving platform */
    uint32_t spawned;
    uint32_t despawned;
    uint32_t pool_full;
    sat_fx16_t top_speed;     /* fastest world speed seen, 16.16 px per step */
    uint8_t cleared;
    uint8_t reserved[3];
} hsp_stats_t;

typedef struct hsp_game {
    sat_terrain_map2_t terrain;

    sat_character2_t hero;
    sat_character2_config_t cfg_stand;
    sat_character2_config_t cfg_roll;
    uint8_t mode;
    uint8_t rolling;
    uint8_t facing_left;
    uint8_t jump_active;
    sat_collider2_t stand;            /* the moving platform the hero rides, or SAT_COLLIDER2_NONE */
    sat_path2_t rail;
    sat_fx16_t rail_distance;
    sat_fx16_t rail_speed;
    int8_t rail_direction;
    uint8_t in_room;
    uint16_t rail_cooldown;           /* steps before the rail can be grabbed again */
    sat_vec2_t respawn;
    sat_clip_player_t hero_clip;

    sat_follow_camera2d_t camera;
    sat_camera2d_t camera_out;

    sat_entity_stream2_t stream;
    uint32_t stream_state[HSP_STREAM_WORDS];
    hsp_object_t objects[HSP_MAX_OBJECTS];

    sat_physics2_world_t world;
    sat_collider2_slot_t slots[HSP_COLLIDER_CAP];
    sat_physics2_pair_t pairs[HSP_COLLIDER_CAP];
    sat_physics2_pair_t next_pairs[HSP_COLLIDER_CAP];
    uint16_t candidates[HSP_COLLIDER_CAP];
    sat_spatial_t spatial;
    uint16_t spatial_heads[48u * 6u];
    sat_spatial_entry_t spatial_entries[HSP_COLLIDER_CAP * 9u];
    uint16_t spatial_stamps[HSP_COLLIDER_CAP];
    sat_box2_t spatial_items[HSP_COLLIDER_CAP];
    sat_collider2_t player_collider;
    sat_collider2_t trigger_collider[HSP_TRIGGER_COUNT];
    hsp_platform_state_t platform[HSP_PLATFORM_COUNT];

    hsp_stats_t stats;
} hsp_game_t;

/* Builds the stage world and puts the hero at the start. The game keeps no pointer to anything
 * but the generated read-only data and itself, so it must not move afterwards. */
sat_result_t hsp_game_init(hsp_game_t* game);

/* One fixed 60 Hz tick: `held` is the pad state, `pressed` its new presses. */
void hsp_game_step(hsp_game_t* game, uint16_t held, uint16_t pressed);

/* The top-left corner of what the camera shows now, shake included (the VDP2 scroll position). */
void hsp_game_view_origin(const hsp_game_t* game, sat_fx16_t* out_x, sat_fx16_t* out_y);

/* Whole pixels per step of the hero's world velocity, 16.16. */
sat_fx16_t hsp_game_hero_speed(const hsp_game_t* game);

/* Where a moving platform is, as a box. */
void hsp_game_platform_box(const hsp_game_t* game, uint32_t index, sat_box2_t* out);

#ifdef __cplusplus
}
#endif

#endif /* HSP_GAME_H */
