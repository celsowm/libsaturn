/* Skybridge 3D internal contract: build-time settings, the world palette,
 * painter passes, shared state and every module entry point. */
#ifndef SKYBRIDGE_H
#define SKYBRIDGE_H
#include <stdint.h>
#include "saturn/saturn.h"
#include "saturn/asset.h"
#include "saturn/scene3d.h"
#include "saturn/follow_camera3d.h"
#include "saturn/anim3d.h"
#include "skybridge_3d/pig_model.h"
#include "saturn/fade3d.h"
#include "saturn/vdp1_color_calc.h"
#include "saturn/vdp2_color_calc.h"
#include "saturn/example_util.h"
#include "saturn/time.h"
#include "saturn/vdp2_rbg0_ground.h"
#include "game.h"
#include "scenery.h"
#include "src/core/parallel/test_faults.h"
#include "src/graphics/3d/scene/test_metrics.h"

#ifndef SAT_SKYBRIDGE_VALIDATION
#define SAT_SKYBRIDGE_VALIDATION 0
#endif
#ifndef SAT_SKYBRIDGE_FORCE_GEM_SPLIT
#define SAT_SKYBRIDGE_FORCE_GEM_SPLIT 0
#endif
#ifndef SAT_PARALLEL_TEST_FAULT
#define SAT_PARALLEL_TEST_FAULT 0
#endif

#define W 320u
#define H 224u
#define HORIZON 96u
#define SEA_WORD 0x00000u
#define ROT_WORD 0x10000u
#define COEF_WORD 0x12000u
#define SKY_W SB_SKY_W
#define SKY_H SB_SKY_H
/* The world palette, declared ONCE. This list both registers the indexed
 * materials at startup and names them at every draw site.
 *
 * It replaces a 36-entry colour table plus a runtime nearest-colour search:
 * drawing code used to pass a raw RGB555 that was snapped to whichever entry
 * happened to be closest, so a colour could be authored in one place and
 * rendered as another. Ten did, most visibly the slick deck's ice blue
 * (9,26,30), which shipped as the green (12,26,19). Thirteen entries -- the
 * pink ramp left over from the procedural pig, replaced by the imported GLB's
 * own shade palette -- were unreachable and merely consumed CRAM.
 * A name resolves at compile time and cannot drift. X(name, r, g, b) */
#define SB_WORLD_PALETTE(X) \
    X(DECK_START_TOP,    24,23,16) \
    X(DECK_MID_TOP,      12,26,19) \
    X(DECK_END_TOP,      27,23,16) \
    X(DECK_ICE_TOP,       9,26,30) \
    X(DECK_GRIP_TOP,     31,20, 7) \
    X(COLLAPSE_WARN_A,   31, 8, 5) \
    X(COLLAPSE_WARN_B,   31,26, 5) \
    X(STEEL_DARK,        12,14,14) \
    X(TEAL_DEEP,          6,16,15) \
    X(STEEL_LIGHT,       17,17,14) \
    X(TEAL_DARK,          8,19,18) \
    X(AMBER_TRIM,        24,20,10) \
    X(MARKER_YELLOW,     31,26, 3) \
    X(LIFT_FRONT,        31,19, 2) \
    X(CHECKPOINT_SIDE,   23,16, 3) \
    X(FINISH_POST_SIDE,  16,12, 3) \
    X(FINISH_POST_FRONT, 27,20, 4) \
    X(FINISH_BAR_SIDE,   20,14, 3) \
    X(FINISH_BAR_FRONT,  27,19, 4) \
    X(HOLE_RIM,          31,23, 3) \
    X(SEESAW_TOP_ODD,    25,20, 9) \
    X(SEESAW_TOP_EVEN,   13,25,25) \
    X(SEESAW_SIDE_ODD,   17,13, 8) \
    X(SEESAW_SIDE_EVEN,   8,16,19) \
    X(SEESAW_HINGE,      31,27, 8) \
    X(SEESAW_PIVOT_TOP,  22,20,14) \
    X(SEESAW_PIVOT_FRONT, 18,17,12) \
    X(PIG_SHADOW,         8,10,10) \
    X(GEM_BRIGHT,        31,30, 8) \
    X(GEM_DEEP,          31,13, 3) \
    X(GEM_MID,           31,23, 4)

enum {
#define SB_PALETTE_ENUM(name,r,g,b) SB_C_##name,
    SB_WORLD_PALETTE(SB_PALETTE_ENUM)
#undef SB_PALETTE_ENUM
    SB_WORLD_COLOR_COUNT
};

#define FADE_START SB_FADE_START
#define FADE_END SB_FADE_END
#define VIEW_LIMIT FADE_END
#define FADE_PALETTE_BANK 4u
#define SCENE_MATERIAL_CAP (SB_WORLD_COLOR_COUNT + SKYBRIDGE_PIG_SHADE_COUNT)
#define SOUNDS 6u
#define SOUND_LEN 2048u
#define MUSIC_LEN 32768u
/* Enough for the worst HUD/help/debug text + rects, without sacrificing
 * the END command. World overflow must never prevent the HUD pass. */
#define SB_HUD_COMMAND_RESERVE 192u
#ifndef SAT_SKYBRIDGE_PARALLEL_MODE
#define SAT_SKYBRIDGE_PARALLEL_MODE 2
#endif
#define SB_PARALLEL_TIMEOUT 60000u
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT || \
    SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_ABORT_FAILURE
#define SB_GEM_WAIT_TIMEOUT 1u
#else
#define SB_GEM_WAIT_TIMEOUT SB_PARALLEL_TIMEOUT
#endif
#define SB_GEM_BATCH_CAP (SB_PICKUP_COUNT * GEM_FACE_CAP)
#define GEM_VERTEX_CAP 6u
#define GEM_FACE_CAP 8u
#define PIG_VERTEX_CAP 900u
#define PIG_FACE_CAP 620u
/* World faces + animated pig + all gems, with a bounded reserve for deck
 * subdivision, braces, insets and near-camera clipping planning. */
#define SCENE_FACE_CAP (PIG_FACE_CAP + SB_PICKUP_COUNT*GEM_FACE_CAP + SB_PLATFORM_COUNT*44u + 128u)
/* The native GLB goes through tools/import_model.py at build time.
 * Fail visibly instead of silently recompiling an unexpectedly huge pig. */
_Static_assert(SKYBRIDGE_PIG_VERTEX_COUNT <= PIG_VERTEX_CAP, "pig vertex budget");
_Static_assert(SKYBRIDGE_PIG_FACE_COUNT <= PIG_FACE_CAP, "pig face budget");
_Static_assert(SKYBRIDGE_PIG_ANIMATION_COUNT == 3u, "Walk Idle Jump clips required");
_Static_assert(SCENE_MATERIAL_CAP <= 255u,
               "shared indexed material pool must fit one palette bank");

/* Painter pass of whatever is being submitted right now. Higher passes paint
 * LAST, so this is the sample's stand-in for a depth buffer the hardware does
 * not have: a deck top is one large quad whose average depth competes with an
 * actor standing on it, and once the actor walks far enough along the deck the
 * quad wins and paints over it from the feet up. Separating them by pass makes
 * that impossible. See SB_PASS_*.
 * All decks share one pass, the deck under the player included: a third pass
 * for it (which used to exist) painted that deck over nearer decks it has no
 * business covering, and the painter's depth order already gets the decks right
 * (40 frames along the validation pad script are pixel-identical without it,
 * except one where a far pier's side face used to show through a nearer deck). */
#define SB_PASS_WORLD   0u  /* every deck */
#define SB_PASS_ACTOR   1u  /* shadow, player, gems */

typedef struct sb_frame_metrics {
    uint32_t begin_ms;
    uint32_t input_ms;
    uint32_t animation_ms;
    uint32_t physics_ms;
    uint32_t camera_ms;
    uint32_t geometry_ms;
    uint32_t submission_ms;
    uint32_t independent_master_ms;
    uint32_t wait_ms;
    uint32_t merge_ms;
    uint32_t vdp1_ms;
    uint32_t hud_ms;
    uint32_t frame_cpu_ms;
    uint32_t frame_ms;
    uint32_t master_wait_frt_ticks;
    uint32_t frame_cpu_frt_ticks, frame_frt_ticks;
    uint32_t geometry_prepare_frt_ticks, geometry_merge_frt_ticks;
    uint32_t task_input_publish_frt_ticks, task_submit_frt_ticks;
    uint32_t task_wait_frt_ticks, task_completion_frt_ticks;
    uint32_t task_release_frt_ticks, task_master_frt_ticks;
    uint32_t task_slave_frt_ticks;
    uint16_t prepared_faces;
    uint16_t rendered_faces;
    uint16_t commands;
    uint16_t task_count;
    uint16_t failures;
    uint16_t timeouts;
    uint16_t over_budget;
    uint16_t visible_gems;
    uint16_t master_gem_items;
    uint16_t slave_gem_items;
    uint16_t master_gem_faces;
    uint16_t slave_gem_faces;
    uint16_t gem_merge_batches;
    uint16_t gem_merged_faces;
    uint16_t animation_submitted;
    uint16_t animation_completed;
    uint16_t animation_failed;
    uint16_t animation_master_dispatches;
    uint16_t animation_slave_dispatches;
    uint16_t geometry_submitted;
    uint16_t geometry_completed;
    uint16_t geometry_failed;
    uint16_t geometry_master_dispatches;
    uint16_t geometry_slave_dispatches;
    uint16_t gem_task_state;
    uint16_t gem_sync_fallback_items;
    uint32_t gem_merge_hash;
} sb_frame_metrics_t;

/* ---------------------------------------------------------------------
 * Shared state, each defined by the one module that owns it.
 * ------------------------------------------------------------------- */
/* main.c: game, camera and frame clock. */
extern sb_game_t g_game;
extern sat_camera3d_t g_camera;
extern sat_vec3_t g_eye, g_target;
extern uint32_t g_frame;
extern int16_t g_yaw;
/* world.c: the scene painter and this frame's world-pass status. */
extern sat_scene_t g_scene;
extern sat_scene3d_face_t g_face_items[SCENE_FACE_CAP];
extern uint32_t g_face_keys[SCENE_FACE_CAP];
extern uint16_t g_face_order[SCENE_FACE_CAP];
extern uint8_t g_world_cmd_full;
extern uint16_t g_dbg_faces, g_dbg_face_cap;
extern uint16_t g_dbg_cmds, g_dbg_cmd_cap;
extern uint16_t g_dbg_pig_faces;
extern uint8_t g_dbg_world_full;
extern sat_result_t g_dbg_scene_status;
/* materials.c: the shared indexed material pool and inset regions. */
extern sat_scene3d_material_t g_scene_materials[SCENE_MATERIAL_CAP];
extern sat_scene3d_material_t g_pig_materials[SKYBRIDGE_PIG_SHADE_COUNT];
extern sat_scene3d_solid_pool_t g_solid_pool;
extern sat_indexed_tiled_quad3_t g_tile_regions[3];
/* pig.c / gems.c: animation state and the Slave's gem task. */
extern sat_anim_state_t g_pig_anim;
extern sat_anim_state_t g_pig_render_anim;
extern uint8_t g_gem_slave_pending;
/* metrics.c */
extern sb_frame_metrics_t g_metrics;
extern uint8_t g_parallel_recovery_blocked;
/* hud.c */
extern uint8_t g_show_debug;

/* ---------------------------------------------------------------------
 * Module entry points.
 * ------------------------------------------------------------------- */
/* metrics.c */
void parallel_recovery_failed(void);
uint32_t parallel_slave_task_count(void);
uint32_t parallel_master_task_count(void);
void record_task_dispatch(uint32_t slave_before,uint32_t master_before,
                          uint16_t* slave_count,uint16_t* master_count);
uint32_t hash_word(uint32_t hash,uint32_t value);
void record_gem_batch_merge(const sat_scene3d_prepare_batch_t* batch);
void record_parallel_snapshot(void);
/* telemetry.c */
#if SAT_SKYBRIDGE_VALIDATION
uint16_t sb_test_frt_counter(void);
uint32_t sb_test_frt_delta(uint16_t start);
void initialize_test_telemetry(void);
void write_test_telemetry(const sat_pad_state_t* pad);
#endif
/* hud.c */
void hud_font_init(void);
void loading_frame(const char* stage, uint8_t percent);
void put_text(const char* text, int x, int y);
void put_label(const char* title, uint32_t n, int x, int y);
void draw_hud(void);
/* materials.c */
void init_tile_texture(void);
void init_scene_materials(void);
/* background.c */
void init_cloud_texture(void);
void draw_clouds(void);
void init_sky(void);
void init_sea(void);
void init_background(void);
void animate_sea_palette(uint32_t tick);
void update_rotation(int32_t fx,int32_t fz);
/* audio.c */
void init_audio(void);
void sound_event(uint16_t event);
/* pig.c */
void pig_init(void);
void submit_pig_animation(void);
void prepare_pig_animation_master(void);
void finish_pig_animation(void);
void player_pig(void);
#if SAT_SKYBRIDGE_VALIDATION
uint32_t animation_pose_hash(void);
#endif
/* gems.c */
void gems_init_mesh(void);
void gems_init_batches(void);
void gems_bind_materials(void);
void prepare_gem_geometry(const uint8_t* deck_slot);
void finish_gem_geometry(void);
/* world.c */
uint8_t world_ok(sat_result_t st);
void world_reset_platform_fades(void);
void draw_world(void);

#endif /* SKYBRIDGE_H */
