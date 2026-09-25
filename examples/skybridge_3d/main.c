/* Skybridge 3D: a playable stock-Saturn platformer, VDP1 world + VDP2 sea/sky. */
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
static const uint16_t g_world_colors[SB_WORLD_COLOR_COUNT]={
#define SB_PALETTE_RGB(name,r,g,b) SAT_RGB555(r,g,b),
    SB_WORLD_PALETTE(SB_PALETTE_RGB)
#undef SB_PALETTE_RGB
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

static sb_game_t g_game;
static sat_ascii_font_t g_font;
static sat_vdp1_texture_t g_tile_textures[3];
/* 2x2 8x8 slices of each 16x16 patterned top inset, uploaded at startup.
 * Extra VRAM: 3 themes * 4 tiles * 64 bytes = 768 bytes (INDEX8). */
static sat_vdp1_texture_t g_tile_quadrants[3][4];
static sat_indexed_tiled_quad3_t g_tile_regions[3];
static uint8_t g_tile_quadrant_pixels[8u*8u];
static sat_vdp1_texture_t g_cloud_texture;
static uint8_t g_cloud_pixels[SB_CLOUD_W * SB_CLOUD_H];
/* One bounded colour/texture pool across world, gems and pig. Local pig
 * shade-to-material view keeps the imported animation's immutable shade IDs. */
static sat_vdp1_texture_t g_solid_textures[SCENE_MATERIAL_CAP];
static sat_scene3d_material_t g_scene_materials[SCENE_MATERIAL_CAP];
static sat_scene3d_material_t g_pig_materials[SKYBRIDGE_PIG_SHADE_COUNT];
static sat_resource_plan_t g_resources;
static sat_resource_plan_entry_t g_resource_entries[8];
static uint16_t g_solid_colors[SCENE_MATERIAL_CAP];
static uint8_t g_solid_pixels[8u*8u];
static sat_scene3d_solid_pool_t g_solid_pool;
static uint8_t g_active_fade_slot=SAT_INDEXED_SOLID_OPAQUE;
/* Painter pass of whatever is being submitted right now. Higher passes paint
 * LAST, so this is the sample's stand-in for a depth buffer the hardware does
 * not have: a deck top is one large quad whose average depth competes with an
 * actor standing on it, and once the actor walks far enough along the deck the
 * quad wins and paints over it from the feet up. Separating them by pass makes
 * that impossible. See SB_PASS_*. */
#define SB_PASS_WORLD   0u  /* the other decks */
#define SB_PASS_SUPPORT 1u  /* the deck under the player */
#define SB_PASS_ACTOR   2u  /* shadow, player, gems */
static uint16_t g_active_pass=SB_PASS_WORLD;
/* One byte per deck, owned here and handed to sat_fade3d_slot, which uses it
 * to keep a deck on its current slot until the camera has actually crossed
 * the transition band. */
static uint8_t g_platform_fade[SB_PLATFORM_COUNT];
/* Eight quantized levels onto the eight VDP2 sprite colour-calculation slots,
 * anything nearer than FADE_START drawn as an ordinary opaque sprite, and a
 * two-unit anti-flicker band. The library owns all of that; see the game-only
 * rule left in platform_fade_slot. */
static const sat_fade3d_slots_t g_fade_slots={
    {FADE_START, FADE_END, 8u, SAT_FADE3D_CULL_AFTER_END, 0u},
    SB_F(2), 0u, 1u, 1u, 0u
};
static uint8_t g_tile_pixels[16u*16u];
static uint32_t g_last_ocean_palette_step=0xFFFFFFFFu;
static uint8_t g_sky[SKY_W * SKY_H] __attribute__((section(".wram_l")));
static uint16_t g_sky_colors[256], g_sea_colors[256];
static uint16_t g_map[SAT_VDP2_NBG0_MAP_CELLS] __attribute__((section(".wram_l")));
/* One camera. sat_camera3d_update derives the view-projection from it, so the
 * look_at/perspective/multiply sequence is not spelled out here. */
static sat_camera3d_t g_camera;
static sat_vec3_t g_eye, g_target;
static sat_follow_camera3d_t g_follow_camera;
/* One bounded shared painter owns projection and ordering of the visible
 * faces of platforms, player and pickups in the same render pass. */
static sat_scene3d_face_t g_face_items[SCENE_FACE_CAP] __attribute__((section(".wram_l")));
/* Caller-owned ordering scratch: the painter sorts these indices, so a frame
 * never moves the face records themselves. */
static uint32_t g_face_keys[SCENE_FACE_CAP] __attribute__((section(".wram_l")));
static uint16_t g_face_order[SCENE_FACE_CAP] __attribute__((section(".wram_l")));
static sat_scene_t g_scene;

/* Imported-and-simplified user GLB: caller-owned model, pose and painter
 * scratch. Static mesh indices are copied ONCE; animated vertices are
 * decoded in place on each frame and then transformed into world space. */
/* One immutable local-space gem mesh shared by every collectible; the game
 * updates only its instance position/bob. Renderer owns facet submission. */
static sat_vec3_t g_gem_vertices[GEM_VERTEX_CAP];
static uint16_t g_gem_indices[GEM_FACE_CAP * 4u];
static sat_mesh_t g_gem_mesh;
static uint16_t g_gem_materials[GEM_FACE_CAP];
static sat_mat4_t g_gem_world[SB_PICKUP_COUNT + 1u];
static sat_scene3d_instance_t g_gem_instances[SB_PICKUP_COUNT + 1u];
static sat_projected_vertex_t g_gem_projected[SB_PICKUP_COUNT + 1u][GEM_VERTEX_CAP];
static sat_vec3_t g_gem_world_vertices[SB_PICKUP_COUNT + 1u][GEM_VERTEX_CAP];
static sat_scene3d_prepare_item_t g_gem_batch_items[SB_PICKUP_COUNT];
static sat_scene3d_prepare_item_t g_gem_master_items[SB_PICKUP_COUNT];
static sat_scene3d_prepare_item_t g_gem_slave_items[SB_PICKUP_COUNT];
static sat_scene3d_face_t g_gem_partition_faces[SB_GEM_BATCH_CAP];
static uint32_t g_gem_partition_keys[SB_GEM_BATCH_CAP];
static sat_scene3d_face_t g_gem_master_faces[SB_GEM_BATCH_CAP];
static uint32_t g_gem_master_keys[SB_GEM_BATCH_CAP];
static sat_scene3d_face_t g_gem_slave_faces[SB_GEM_BATCH_CAP];
static uint32_t g_gem_slave_keys[SB_GEM_BATCH_CAP];
static sat_scene3d_prepare_batch_t g_gem_partition_batch;
static sat_scene3d_prepare_batch_t g_gem_master_batch;
static sat_scene3d_prepare_batch_t g_gem_slave_batch;
static sat_parallel_handle_t g_gem_slave_handle;
static uint8_t g_gem_slave_pending;
static uint8_t g_gem_master_ready;
static uint8_t g_gem_slave_ready;
static uint8_t g_gem_master_merged;
/* A failed abort/release is an ownership failure, not a normal task error.
 * Keep all associated storage pinned and stop dispatching until reset. */
static uint8_t g_parallel_recovery_blocked;
static sat_vec3_t g_pig_vertices[PIG_VERTEX_CAP];
static uint16_t g_pig_indices[PIG_FACE_CAP*4u];
static sat_mesh_t g_pig_mesh;
 static sat_projected_vertex_t g_pig_projected[PIG_VERTEX_CAP];
static uint16_t g_pig_face_textures[PIG_FACE_CAP];
static sat_anim_state_t g_pig_anim;
static sat_anim_state_t g_pig_render_anim;
/* The executor retains the complete input graph until terminal completion and
 * release. These are deliberately static: a stack descriptor would become
 * invalid as soon as submit_pig_animation() returned. */
static sat_anim_decode_job_t g_pig_anim_job;
static sat_anim_state_t g_pig_anim_job_state;
static uint8_t g_pig_anim_write_buffer;
static sat_scene3d_instance_t g_pig_instance;
static sat_vec3_t g_pig_pose[2][PIG_VERTEX_CAP];
static uint8_t g_pig_render_buffer;
static sat_parallel_handle_t g_pig_anim_handle;
static uint8_t g_pig_anim_pending;

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
static sb_frame_metrics_t g_metrics;
static uint32_t g_last_parallel_wait;
static uint32_t g_last_parallel_submitted;
static uint32_t g_last_parallel_failed;
static sat_parallel_stats_t g_last_parallel_stats;
static uint32_t g_frame;

#if SAT_SKYBRIDGE_VALIDATION
#define SB_TEST_TELEMETRY_CAPACITY 384u
#define SB_TEST_TELEMETRY_MAGIC 0x5342544Du
typedef struct sb_test_telemetry_record {
    uint32_t serial, frame, game_ticks;
    uint32_t game_hash, animation_hash, merge_order_hash;
    uint32_t frame_cpu_ms, frame_ms, wait_ms;
    uint32_t visible_gems, master_items, slave_items;
    uint32_t master_faces, slave_faces, merge_batches, merged_faces;
    uint32_t animation_submitted, animation_completed, animation_failed;
    uint32_t animation_master, animation_slave;
    uint32_t geometry_submitted, geometry_completed, geometry_failed;
    uint32_t geometry_master, geometry_slave;
    uint32_t timeouts, failures, recovery_blocked, gem_pending;
    uint32_t gem_task_state, fault_mask, sync_fallback_items;
    uint32_t game_course, game_pickups, game_paused, game_finished, game_support;
    uint32_t game_x, game_y, game_z, game_vx, game_vy, game_vz;
    uint32_t animation_clip, animation_frame, animation_time;
    uint32_t pad_held, pad_pressed;
    uint32_t frame_cpu_frt_ticks, frame_frt_ticks;
    uint32_t geometry_prepare_frt_ticks, geometry_merge_frt_ticks;
    uint32_t task_input_publish_frt_ticks, task_submit_frt_ticks;
    uint32_t task_wait_frt_ticks, task_completion_frt_ticks;
    uint32_t task_release_frt_ticks, task_master_frt_ticks;
    uint32_t task_slave_frt_ticks;
    uint32_t scene_painter_frt_ticks, scene_emit_frt_ticks;
    uint32_t scene_command_hash, scene_command_count, scene_command_capacity;
    uint32_t timer_read_overhead_frt_ticks;
} sb_test_telemetry_record_t;
typedef struct sb_test_telemetry_block {
    uint32_t magic, version, record_words, capacity, write_count;
    sb_test_telemetry_record_t records[SB_TEST_TELEMETRY_CAPACITY];
} sb_test_telemetry_block_t;
/* This global is intentionally named and retained so run-harness.ps1 can
 * resolve its linked WRAM address from the GNU ld map in validation builds. */
sb_test_telemetry_block_t g_sb_test_telemetry __attribute__((used));
static uint32_t g_test_timer_read_overhead_ticks;

static uint16_t sb_test_frt_counter(void) {
    volatile uint8_t* const high=(volatile uint8_t*)0xFFFFFE12u;
    volatile uint8_t* const low=(volatile uint8_t*)0xFFFFFE13u;
    const uint16_t h=*high;
    return (uint16_t)((h<<8u)|*low);
}

static uint32_t sb_test_frt_delta(uint16_t start) {
    return (uint16_t)(sb_test_frt_counter()-start);
}
#endif

static void parallel_recovery_failed(void) {
    g_parallel_recovery_blocked=1u;
    ++g_metrics.failures;
}

static uint32_t parallel_slave_task_count(void) {
    sat_parallel_stats_t stats={0};
    return sat_parallel_stats(&stats)==SAT_OK?stats.slave_tasks:0u;
}

static uint32_t parallel_master_task_count(void) {
    sat_parallel_stats_t stats={0};
    return sat_parallel_stats(&stats)==SAT_OK?stats.master_tasks:0u;
}

static void record_task_dispatch(uint32_t slave_before,uint32_t master_before,
                                 uint16_t* slave_count,uint16_t* master_count) {
    const uint32_t slave_after=parallel_slave_task_count();
    const uint32_t master_after=parallel_master_task_count();
    if(slave_after!=slave_before)++*slave_count;
    else if(master_after!=master_before)++*master_count;
}

static uint32_t hash_word(uint32_t hash,uint32_t value) {
    for(uint8_t byte=0u;byte<4u;++byte) {
        hash^=(value>>(24u-(uint32_t)byte*8u))&0xFFu;
        hash*=16777619u;
    }
    return hash;
}

static uint32_t hash_scene_face(uint32_t hash,const sat_scene3d_face_t* face,
                                uint32_t key) {
    hash=hash_word(hash,key);
    hash=hash_word(hash,face->projected_safe);
    hash=hash_word(hash,face->gouraud_valid);
    hash=hash_word(hash,face->material.kind);
    hash=hash_word(hash,face->material.rgb555);
    hash=hash_word(hash,face->material.color_calc_slot);
    for(uint8_t vertex=0u;vertex<4u;++vertex) {
        /* world is stored only for the clipping fallback. */
        if(!face->projected_safe) {
            hash=hash_word(hash,(uint32_t)face->world.v[vertex].x);
            hash=hash_word(hash,(uint32_t)face->world.v[vertex].y);
            hash=hash_word(hash,(uint32_t)face->world.v[vertex].z);
        }
        hash=hash_word(hash,(uint16_t)face->projected.x[vertex]);
        hash=hash_word(hash,(uint16_t)face->projected.y[vertex]);
        if(face->gouraud_valid)hash=hash_word(hash,face->gouraud[vertex]);
    }
    if(face->material.texture!=0) {
        const sat_vdp1_texture_t* texture=face->material.texture;
        hash=hash_word(hash,texture->srca);
        hash=hash_word(hash,texture->width);
        hash=hash_word(hash,texture->height);
        hash=hash_word(hash,texture->palette);
        hash=hash_word(hash,texture->valid);
    }
    return hash;
}

static void record_gem_batch_merge(const sat_scene3d_prepare_batch_t* batch) {
    uint16_t i;
    if(g_metrics.gem_merge_hash==0u)g_metrics.gem_merge_hash=2166136261u;
    for(i=0u;i<batch->metrics.prepared_faces;++i) {
        g_metrics.gem_merge_hash=hash_scene_face(g_metrics.gem_merge_hash,
                                                  &batch->faces[i],batch->keys[i]);
    }
    ++g_metrics.gem_merge_batches;
    g_metrics.gem_merged_faces=(uint16_t)(g_metrics.gem_merged_faces+
                                           batch->metrics.prepared_faces);
}

#if SAT_SKYBRIDGE_VALIDATION
static uint32_t game_state_hash(void) {
    uint32_t hash=2166136261u;
    hash=hash_word(hash,(uint32_t)g_game.x);hash=hash_word(hash,(uint32_t)g_game.y);
    hash=hash_word(hash,(uint32_t)g_game.z);hash=hash_word(hash,(uint32_t)g_game.vx);
    hash=hash_word(hash,(uint32_t)g_game.vy);hash=hash_word(hash,(uint32_t)g_game.vz);
    hash=hash_word(hash,(uint32_t)g_game.moving_x);hash=hash_word(hash,g_game.ticks);
    hash=hash_word(hash,g_game.pickups);hash=hash_word(hash,g_game.collapse_ticks);
    hash=hash_word(hash,g_game.checkpoint);hash=hash_word(hash,g_game.course);
    hash=hash_word(hash,g_game.coyote);hash=hash_word(hash,g_game.jump_buffer);
    hash=hash_word(hash,g_game.finished);hash=hash_word(hash,g_game.paused);
    hash=hash_word(hash,(uint8_t)g_game.facing_x);hash=hash_word(hash,(uint8_t)g_game.facing_z);
    hash=hash_word(hash,(uint8_t)g_game.support);
    for(uint8_t i=0u;i<SB_PLATFORM_COUNT;++i)
        hash=hash_word(hash,(uint32_t)g_game.seesaw_tilt[i]);
    return hash;
}

static uint32_t animation_pose_hash(void) {
    uint32_t hash=2166136261u;
    const sat_vec3_t* pose=g_pig_pose[g_pig_render_buffer];
    hash=hash_word(hash,g_pig_render_anim.clip);
    hash=hash_word(hash,g_pig_render_anim.frame);
    hash=hash_word(hash,(uint32_t)g_pig_render_anim.time);
    for(uint16_t i=0u;i<PIG_VERTEX_CAP;++i) {
        hash=hash_word(hash,(uint32_t)pose[i].x);
        hash=hash_word(hash,(uint32_t)pose[i].y);
        hash=hash_word(hash,(uint32_t)pose[i].z);
    }
    return hash;
}

static void initialize_test_telemetry(void) {
    g_sb_test_telemetry.magic=SB_TEST_TELEMETRY_MAGIC;
    g_sb_test_telemetry.version=5u;
    g_sb_test_telemetry.record_words=
        (uint32_t)(sizeof(sb_test_telemetry_record_t)/sizeof(uint32_t));
    g_sb_test_telemetry.capacity=SB_TEST_TELEMETRY_CAPACITY;
    g_sb_test_telemetry.write_count=0u;
    {
        const uint16_t start=sb_test_frt_counter();
        volatile uint16_t sample=0u;
        for(uint16_t i=0u;i<256u;++i)sample^=sb_test_frt_counter();
        (void)sample;
        /* Upper bound: includes loop overhead and 256 counter reads. */
        g_test_timer_read_overhead_ticks=(sb_test_frt_delta(start)+255u)/256u;
    }
}

static void write_test_telemetry(const sat_pad_state_t* pad) {
    const uint32_t serial=g_sb_test_telemetry.write_count;
    sb_test_telemetry_record_t* record=
        &g_sb_test_telemetry.records[serial%SB_TEST_TELEMETRY_CAPACITY];
    *record=(sb_test_telemetry_record_t){
        serial,g_frame,g_game.ticks,game_state_hash(),animation_pose_hash(),
        g_metrics.gem_merge_hash!=0u?g_metrics.gem_merge_hash:2166136261u,
        g_metrics.frame_cpu_ms,g_metrics.frame_ms,g_metrics.wait_ms,
        g_metrics.visible_gems,g_metrics.master_gem_items,g_metrics.slave_gem_items,
        g_metrics.master_gem_faces,g_metrics.slave_gem_faces,
        g_metrics.gem_merge_batches,g_metrics.gem_merged_faces,
        g_metrics.animation_submitted,g_metrics.animation_completed,
        g_metrics.animation_failed,g_metrics.animation_master_dispatches,
        g_metrics.animation_slave_dispatches,
        g_metrics.geometry_submitted,g_metrics.geometry_completed,
        g_metrics.geometry_failed,g_metrics.geometry_master_dispatches,
        g_metrics.geometry_slave_dispatches,g_metrics.timeouts,g_metrics.failures,
        g_parallel_recovery_blocked,g_gem_slave_pending,g_metrics.gem_task_state,
        sat_parallel_test_fault_fired(),g_metrics.gem_sync_fallback_items,
        g_game.course,g_game.pickups,g_game.paused,g_game.finished,
        (uint8_t)g_game.support,(uint32_t)g_game.x,(uint32_t)g_game.y,
        (uint32_t)g_game.z,(uint32_t)g_game.vx,(uint32_t)g_game.vy,
        (uint32_t)g_game.vz,g_pig_render_anim.clip,g_pig_render_anim.frame,
        (uint32_t)g_pig_render_anim.time,pad->held,pad->pressed,
        g_metrics.frame_cpu_frt_ticks,g_metrics.frame_frt_ticks,
        g_metrics.geometry_prepare_frt_ticks,g_metrics.geometry_merge_frt_ticks,
        g_metrics.task_input_publish_frt_ticks,g_metrics.task_submit_frt_ticks,
        g_metrics.task_wait_frt_ticks,g_metrics.task_completion_frt_ticks,
        g_metrics.task_release_frt_ticks,g_metrics.task_master_frt_ticks,
        g_metrics.task_slave_frt_ticks,sat_scene3d_test_painter_ticks(),
        sat_scene3d_test_emit_ticks(),sat_vdp1_test_scene_command_hash(),
        sat_vdp1_test_scene_command_count(),
        sat_vdp1_test_scene_command_capacity(),g_test_timer_read_overhead_ticks};
    __asm__ volatile("" ::: "memory");
    g_sb_test_telemetry.write_count=serial+1u;
}
#endif

static int16_t g_yaw;
static sat_step_clock_t g_step_clock;
static sat_sound_t g_sounds[SOUNDS];
static sat_voice_t g_music_voice;
static const char* const g_sfx_paths[SOUNDS-1u]={
    "skybridge/sfx/jump", "skybridge/sfx/land", "skybridge/sfx/pickup",
    "skybridge/sfx/fall", "skybridge/sfx/finish"
};
static int8_t g_audio[SOUNDS - 1u][SOUND_LEN] __attribute__((section(".wram_l")));
static int8_t g_music[MUSIC_LEN] __attribute__((section(".wram_l")));
static uint8_t g_audio_ready;
static uint8_t g_show_help=1u;
static uint8_t g_show_debug=0u;
static uint8_t g_world_cmd_full=0u;
/* Budget telemetry for the debug overlay. A frame that runs out of face slots
 * or VDP1 commands drops geometry SILENTLY -- world_ok() treats that as an
 * optional decoration -- so without these counters a half-drawn character
 * looks like a clipping or animation bug. Captured before the flush resets
 * the painter, and before the HUD pass spends its own reserve. */
static uint16_t g_dbg_faces, g_dbg_face_cap;
static uint16_t g_dbg_cmds, g_dbg_cmd_cap;
static uint16_t g_dbg_pig_faces;
static uint8_t g_dbg_world_full;
static sat_result_t g_dbg_scene_status=SAT_OK;
static const sat_vdp2_rbg0_ground_config_t g_ocean = {
    512u, 256u, 160u, HORIZON, 96u, 8u, 96u, COEF_WORD
};
static sat_vdp2_ground_environment_t g_environment;
static uint16_t g_environment_coefficients[H * 2u];
static uint16_t g_environment_params[48];

/* Boot is a series of completed jobs, not a pretend timed progress bar.
 * VDP1 draws this fullscreen panel independently of whether VDP2 is ready. */
static void loading_frame(const char* stage, uint8_t percent) {
    sat_example_loading_frame(&g_font,"SKYBRIDGE 3D",stage,percent,W,H);
}
static void put_text(const char* text, int x, int y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, x, y, 8, 0u, 0u);
}
static void label(const char* title, uint32_t n, int x, int y) {
    (void)sat_ascii_font_draw_label_u32(&g_font,title,n,x,y,8,0u,0u);
}
/* Display signed 16.16 WORLD coordinates, to one decimal place. The axis
 * letter is the only part the HUD still assembles; sat_fmt_fx16 owns the
 * number, including keeping the sign on -0.x during a fall. */
static void hud_coord(char axis,int32_t fixed,int x,int y) {
    char out[2u+SAT_FMT_FX16_MAX];
    out[0]=axis;
    out[1]=' ';
    if(sat_fmt_fx16(fixed,1u,out+2,sizeof(out)-2u,0)!=SAT_OK)return;
    put_text(out,x,y);
}
/* Sprite Type 0's RGB-coded pixels have different VDP2 interpretation
 * from indexed pixels. With sprite color calc enabled, mixing RGB geometry
 * and indexed faded quads made the near player's RGB body and the solid
 * platform walls disappear over the RBG0 ocean on the target emulator.
 * Keep *every* world-facing material on a single indexed palette path.
 * Ordinary sprites remain opaque at priority 7; only distant objects
 * explicitly request the faded selector (priority 6, above sea priority 5).
 * The indexed top insets keep their original patterned texture. */
/* Every world submission shares one outcome policy, so it is stated once.
 * A full command list or face queue ends the frame's world pass quietly:
 * the decorations that would follow are optional, and a partially drawn
 * frame is not an error. An unsupported face is simply skipped. Anything
 * else is a genuine bug and must not be swallowed. Returns 0 once the
 * world pass is closed, so a caller can stop early. */
static uint8_t world_ok(sat_result_t st) {
    if(st==SAT_ERR_CAPACITY) {g_world_cmd_full=1u;return 0u;}
    if(st!=SAT_OK && st!=SAT_ERR_UNSUPPORTED) {
        ++g_metrics.failures;
        g_dbg_scene_status=st;
        return 0u;
    }
    return 1u;
}
/* Skybridge only selects a level material; the shared painter owns the
 * camera projection, per-face ordering, clipping and VDP1 submission. */
/* `color` is an SB_C_* palette name, i.e. the pool index the startup
 * registration produced -- no lookup, no snapping. */
static void put_quad(const sat_quad3_t* q, uint8_t color) {
    if(g_world_cmd_full) return;
    sat_scene3d_material_t material=g_scene_materials[color];
    material.color_calc_slot=g_active_fade_slot;
    world_ok(sat_scene_submit_quad(
        &g_scene,q,&material,g_active_pass));
}
static void put_quad_lit(const sat_quad3_t* q, uint8_t c) {
    /* Gouraud RGB polygons are incompatible with this VDP2 blend path;
     * use an indexed flat-colored face so near geometry stays solid. */
    put_quad(q,c);
}
static void pquad(sat_quad3_t* q, int32_t ax,int32_t ay,int32_t az,
                  int32_t bx,int32_t by,int32_t bz,
                  int32_t cx,int32_t cy,int32_t cz,
                  int32_t dx,int32_t dy,int32_t dz) {
    q->v[0]=(sat_vec3_t){ax,ay,az};
    q->v[1]=(sat_vec3_t){bx,by,bz};
    q->v[2]=(sat_vec3_t){cx,cy,cz};
    q->v[3]=(sat_vec3_t){dx,dy,dz};
}
static void quad_rect_xz(sat_quad3_t* q,int32_t lx,int32_t rx,int32_t z0,int32_t z1,int32_t y) {
    pquad(q,lx,y,z0, rx,y,z0, rx,y,z1, lx,y,z1);
}
/* Rims, edge bands and the contact shadow are all one flat XZ rectangle
 * submitted right where it is built; building and submitting separately
 * only invites the two to drift apart. */
static void put_rect_xz(int32_t lx,int32_t rx,int32_t z0,int32_t z1,
                        int32_t y,uint8_t c) {
    sat_quad3_t q;
    quad_rect_xz(&q,lx,rx,z0,z1,y);
    put_quad(&q,c);
}
/* Draw just the top and camera-facing sides, always with consistent thickness. */
/* Level code describes a box, not its camera-facing vertices or winding.
 * The renderer owns side selection, safe near/screen clipping and materials. */
static void box3(int32_t x,int32_t y,int32_t z,int32_t hx,int32_t hy,int32_t hz,
                 uint8_t top,uint8_t xcolor,uint8_t zcolor,uint8_t trim) {
    if(g_world_cmd_full) return;
    sat_indexed_box3_t block={0};
    block.top_center=(sat_vec3_t){x,y,z};
    block.half_extents=(sat_vec3_t){hx,hy,hz};
    block.top_material=g_scene_materials[top].texture;
    block.x_material=g_scene_materials[xcolor].texture;
    block.z_material=g_scene_materials[zcolor].texture;
    if(!world_ok(sat_scene_submit_box(
            &g_scene,&block,g_active_fade_slot,g_active_pass))) return;
    /* Decorative inset remains level-owned, not a fake collision surface. */
    if(g_eye.y<=y || !trim || hx<=SB_F(4) || hz<=SB_F(4)) return;
    sat_quad3_t q;
    quad_rect_xz(&q,x-hx+SB_F(2),x+hx-SB_F(2),
                    z-hz+SB_F(2),z+hz-SB_F(2),y+SB_F(1)/32);
    if(trim==2u) {
        put_quad(&q,SB_C_AMBER_TRIM);
        return;
    }
    const uint8_t theme=trim==1u?0u:(trim==3u?1u:2u);
    world_ok(sat_scene_submit_tiled_quad(
        &g_scene,&q,&g_tile_regions[theme],g_active_fade_slot,
        g_active_pass));
}
/* The three deck zones -- start (0-3), middle (4-6) and finish (7-9) --
 * are what every platform tint keys off, so the split lives in one place
 * instead of being re-spelled at each colour decision. */
static uint8_t deck_tint(uint8_t i,uint8_t start,uint8_t mid,uint8_t end) {
    return i<4u?start:(i<7u?mid:end);
}
static uint8_t top_color(uint8_t i) {
    return deck_tint(i,SB_C_DECK_START_TOP,
                       SB_C_DECK_MID_TOP,
                       SB_C_DECK_END_TOP);
}
/* Palette-to-material mapping is baked once at initialization. */
static const uint8_t g_gem_facet_colors[GEM_FACE_CAP]={
    SB_C_GEM_BRIGHT,SB_C_GEM_DEEP,
    SB_C_MARKER_YELLOW,SB_C_GEM_MID,
    SB_C_GEM_BRIGHT,SB_C_GEM_DEEP,
    SB_C_GEM_MID,SB_C_GEM_MID
};
/* The library owns the facet painter and clipping; game logic only positions
 * an instance of the immutable octahedron mesh above its supporting deck. */
/* An actual hinged 3D deck: the two long ends use the SAME 16.16
 * surface-height function as the landing solver. It is a sloped quad,
 * not a flat box translated or rotated just for the camera. Only three
 * visible side faces and a small hinge are needed on Saturn hardware. */
static void draw_seesaw(uint8_t id,const sb_platform_t* p) {
    const int32_t x=sb_platform_x(&g_game,id),z=SB_F(p->z);
    const int32_t lx=x-SB_F(p->half_x),rx=x+SB_F(p->half_x);
    const int32_t bz=z-SB_F(p->half_z),fz=z+SB_F(p->half_z);
    const int32_t y0=sb_platform_surface_y(&g_game,id,x,bz);
    const int32_t y1=sb_platform_surface_y(&g_game,id,x,fz);
    const int32_t bottom=sb_platform_y(&g_game,id)-SB_F(5);
    const uint16_t top=(id&1u)?SB_C_SEESAW_TOP_ODD:
                                  SB_C_SEESAW_TOP_EVEN;
    const uint16_t side=(id&1u)?SB_C_SEESAW_SIDE_ODD:
                                   SB_C_SEESAW_SIDE_EVEN;
    sat_quad3_t q;
    if(g_eye.x>x) {
        pquad(&q,rx,y0,bz,rx,y1,fz,rx,bottom,fz,rx,bottom,bz);
    } else {
        pquad(&q,lx,y1,fz,lx,y0,bz,lx,bottom,bz,lx,bottom,fz);
    }
    put_quad(&q,side);
    if(g_eye.z>z)
        pquad(&q,rx,y1,fz,lx,y1,fz,lx,bottom,fz,rx,bottom,fz);
    else
        pquad(&q,lx,y0,bz,rx,y0,bz,rx,bottom,bz,lx,bottom,bz);
    put_quad(&q,side);
    if(g_eye.y>bottom) {
        pquad(&q,lx,y0,bz,rx,y0,bz,rx,y1,fz,lx,y1,fz);
        put_quad_lit(&q,top);
        /* The short crossbar remains on the hinge, identifying the pivot
         * while the opposite ends visibly rise and fall. */
        pquad(&q,lx,sb_platform_y(&g_game,id)+SB_F(1)/20,z-SB_F(1)/3,
              rx,sb_platform_y(&g_game,id)+SB_F(1)/20,z-SB_F(1)/3,
              rx,sb_platform_y(&g_game,id)+SB_F(1)/20,z+SB_F(1)/3,
              lx,sb_platform_y(&g_game,id)+SB_F(1)/20,z+SB_F(1)/3);
        put_quad(&q,SB_C_SEESAW_HINGE);
    }
    box3(x,bottom-SB_F(3),z,SB_F(2),SB_F(3),SB_F(2),
         SB_C_SEESAW_PIVOT_TOP,SB_C_STEEL_DARK,
         SB_C_SEESAW_PIVOT_FRONT,0u);
}
static void stage_box(uint8_t i) {
    const sb_platform_t* p=&sb_course_platforms(&g_game)[i];
    if(p->kind==SB_SEESAW) {
        draw_seesaw(i,p);
        return;
    }
    int32_t x=sb_platform_x(&g_game,i), z=SB_F(p->z);
    int32_t top_y=sb_platform_y(&g_game,i);
    uint16_t t=top_color(i);
    if(p->surface==SB_SURFACE_SLICK)t=SB_C_DECK_ICE_TOP;
    if(p->surface==SB_SURFACE_GRIP)t=SB_C_DECK_GRIP_TOP;
    if(p->kind==SB_LIFT)
        t=SB_C_MARKER_YELLOW;
    if (p->kind==SB_COLLAPSING && g_game.collapse_ticks>0u &&
        g_game.collapse_ticks<=30u)
        t=(g_game.ticks&4u)?SB_C_COLLAPSE_WARN_A:SB_C_COLLAPSE_WARN_B;
    {
        /* Course 3 has actual missing geometry, not a dark quad drawn over
         * an intact floor. Use the SAME shared solid slices as ground
         * collision, leaving the rectangular opening open to the ocean. */
        const sb_hole_t* hole=sb_platform_hole(&g_game,i);
        sb_deck_slice_t slabs[4];
        uint8_t pieces=sb_deck_slices(&g_game,i,slabs);
        uint16_t side=p->kind==SB_LIFT?SB_C_AMBER_TRIM:
            (i<4u?SB_C_STEEL_DARK:SB_C_TEAL_DEEP);
        uint16_t front=p->kind==SB_LIFT?SB_C_LIFT_FRONT:
            (i<4u?SB_C_STEEL_LIGHT:SB_C_TEAL_DARK);
        for(uint8_t part=0u;part<pieces;++part) {
            const sb_deck_slice_t* s=&slabs[part];
            int32_t half_x=(s->max_x-s->min_x)/2;
            int32_t half_z=(s->max_z-s->min_z)/2;
            box3(s->min_x+half_x,top_y,s->min_z+half_z,
                 half_x,SB_F(5),half_z,t,side,front,
                 hole?0u:(p->kind==SB_COLLAPSING?2u:
                     (p->kind==SB_LIFT?4u:(i<4u?1u:(i<7u?3u:4u)))));
        }
        if(hole && g_eye.y>top_y) {
            /* Bright narrow rim is drawn OUTSIDE the void. These four
             * strips never cover the aperture or create phantom flooring. */
            const int32_t hl=x+SB_F(hole->x_offset-hole->half_x);
            const int32_t hr=x+SB_F(hole->x_offset+hole->half_x);
            const int32_t hb=z+SB_F(hole->z_offset-hole->half_z);
            const int32_t hf=z+SB_F(hole->z_offset+hole->half_z);
            const int32_t y=top_y+SB_F(1)/24;
            const uint16_t warning=SB_C_HOLE_RIM;
            put_rect_xz(hl-SB_F(1),hl,hb,hf,y,warning);
            put_rect_xz(hr,hr+SB_F(1),hb,hf,y,warning);
            put_rect_xz(hl,hr,hb-SB_F(1),hb,y,warning);
            put_rect_xz(hl,hr,hf,hf+SB_F(1),y,warning);
        }
    }
    /* Distinct corner braces and inset deck rails make the floating decks
     * read as engineered 3D structures rather than untextured slabs.
     * All braces are outside the traversable deck and are decorative only. */
    if(i==0u || i==3u || i==4u || i==6u || i==9u) {
        int32_t rim_z=z+(g_eye.z<z?-SB_F(p->half_z-3):
                                      SB_F(p->half_z-3));
        uint16_t metal=deck_tint(i,SB_C_STEEL_LIGHT,
                                   SB_C_TEAL_DARK,
                                   SB_C_AMBER_TRIM);
        box3(x-SB_F(p->half_x-4),top_y-SB_F(6),rim_z,
             SB_F(2),SB_F(3),SB_F(2),metal,metal,metal,0u);
        box3(x+SB_F(p->half_x-4),top_y-SB_F(6),rim_z,
             SB_F(2),SB_F(3),SB_F(2),metal,metal,metal,0u);
    }
    /* A thin raised, contrasting edge band makes platform boundaries
     * legible at speed without adding collision-changing obstacles. */
    if(g_eye.y>top_y && p->half_x>7 && p->half_z>7) {
        int32_t sy=top_y+SB_F(1)/24;
        int32_t lx=x-SB_F(p->half_x-1),rx=x+SB_F(p->half_x-1);
        int32_t bz=z-SB_F(p->half_z-1),fz=z+SB_F(p->half_z-1);
        uint16_t edge_color=deck_tint(i,SB_C_AMBER_TRIM,
                                        SB_C_DECK_MID_TOP,
                                        SB_C_MARKER_YELLOW);
        put_rect_xz(lx,rx,bz,bz+SB_F(1),sy,edge_color);
        put_rect_xz(lx,rx,fz-SB_F(1),fz,sy,edge_color);
    }
    /* Elevators have a visible shaft below the deck: its length
     * changes with the actual collision top, never a separate animation. */
    if(p->kind==SB_LIFT) {
        box3(x,top_y-SB_F(9),z,SB_F(2),SB_F(4),SB_F(2),
             SB_C_AMBER_TRIM,SB_C_STEEL_DARK,
             SB_C_STEEL_LIGHT,0u);
    }
    /* Checkpoints and finish are physically marked, not just HUD text. */
    if (i==3u || i==6u) {
        box3(x-SB_F(7),top_y+SB_F(5),z+SB_F(5),SB_F(1),SB_F(5),SB_F(1),
             SB_C_MARKER_YELLOW,SB_C_CHECKPOINT_SIDE,SB_C_LIFT_FRONT,0u);
    }
    if (i==9u) {
        box3(x-SB_F(10),top_y+SB_F(9),z+SB_F(5),SB_F(1),SB_F(9),SB_F(1),
             SB_C_MARKER_YELLOW,SB_C_FINISH_POST_SIDE,SB_C_FINISH_POST_FRONT,0u);
        box3(x+SB_F(10),top_y+SB_F(9),z+SB_F(5),SB_F(1),SB_F(9),SB_F(1),
             SB_C_MARKER_YELLOW,SB_C_FINISH_POST_SIDE,SB_C_FINISH_POST_FRONT,0u);
        box3(x,top_y+SB_F(19),z+SB_F(5),SB_F(11),SB_F(1),SB_F(1),
             SB_C_MARKER_YELLOW,SB_C_FINISH_BAR_SIDE,SB_C_FINISH_BAR_FRONT,0u);
    }
}
/* Model only: the pig is contained inside the original 4x4x5 fixed-point
 * collision volume. Course 1/2 movement, coyote time, gem contact and
 * elevator carry remain owned by game.h. Its geometry uses indexed VDP1
 * distorted sprites through box3()/put_quad(), never RGB faces that break
 * the existing VDP2 distance color-calculation setup. */
/* Preserve the existing collision/contact shadow, not the old procedural
 * pig body. The visible pig now comes exclusively from the user-modified
 * CC BY 4.0 GLB converted by the stock importer at build time. */
static void pig_shadow(const uint8_t* deck_slot) {
    if(g_game.support>=0 &&
       deck_slot[(uint8_t)g_game.support]!=SAT_FADE3D_SLOT_CULLED) {
        const int32_t px=g_game.x,pz=g_game.z;
        const int32_t sy=sb_platform_surface_y(
            &g_game,(uint8_t)g_game.support,px,pz)+SB_F(1)/16;
        /* The shadow is a mark ON the deck, so it takes the deck's slot and
         * is skipped entirely when the deck itself was not drawn -- a shadow
         * floating over open sea reads as a bug. */
        const uint8_t previous=g_active_fade_slot;
        g_active_fade_slot=deck_slot[(uint8_t)g_game.support];
        put_rect_xz(px-SB_F(2),px+SB_F(2),
                    pz-SB_F(2),pz+SB_F(2),sy,SB_C_PIG_SHADOW);
        g_active_fade_slot=previous;
    }
}
static void record_parallel_snapshot(void) {
    sat_parallel_stats_t stats={0};
    const uint32_t local_failures=g_metrics.failures;
    if(sat_parallel_stats(&stats)!=SAT_OK)return;
    g_metrics.task_count=(uint16_t)((stats.submitted-g_last_parallel_submitted)>0xFFFFu
        ?0xFFFFu:stats.submitted-g_last_parallel_submitted);
    {
        const uint32_t failures=local_failures+
            (stats.failed-g_last_parallel_failed);
        g_metrics.failures=(uint16_t)(failures>0xFFFFu?0xFFFFu:failures);
    }
    /* master_wait_ticks is a raw SH-2 FRT delta. Keep it separate from the
     * millisecond samples collected around the actual wait calls below. */
    g_metrics.master_wait_frt_ticks=stats.master_wait_ticks-g_last_parallel_wait;
    g_metrics.task_wait_frt_ticks=stats.master_wait_ticks-
        g_last_parallel_stats.master_wait_ticks;
    g_metrics.task_completion_frt_ticks=stats.completion_ticks-
        g_last_parallel_stats.completion_ticks;
    g_metrics.task_master_frt_ticks=stats.master_task_ticks-
        g_last_parallel_stats.master_task_ticks;
    g_metrics.task_slave_frt_ticks=stats.slave_task_ticks-
        g_last_parallel_stats.slave_task_ticks;
    g_last_parallel_stats=stats;
    g_last_parallel_wait=stats.master_wait_ticks;
    g_last_parallel_submitted=stats.submitted;
    g_last_parallel_failed=stats.failed;
}

static void submit_pig_animation(void) {
    const uint8_t write_buffer=(uint8_t)(1u-g_pig_render_buffer);
    const uint32_t slave_before=parallel_slave_task_count();
    const uint32_t master_before=parallel_master_task_count();
    if(g_pig_anim_pending || g_parallel_recovery_blocked)return;
    g_pig_anim_job_state=g_pig_anim;
    g_pig_anim_write_buffer=write_buffer;
    g_pig_anim_job=(sat_anim_decode_job_t){&skybridge_pig_anim_asset,
        &g_pig_anim_job_state,g_pig_pose[write_buffer],PIG_VERTEX_CAP,0u};
#if SAT_SKYBRIDGE_VALIDATION
    const uint16_t submit_start=sb_test_frt_counter();
#endif
    const sat_result_t submitted=
        sat_anim_decode_async(&g_pig_anim_job,&g_pig_anim_handle);
#if SAT_SKYBRIDGE_VALIDATION
    g_metrics.task_submit_frt_ticks+=(uint16_t)(
        sb_test_frt_counter()-submit_start);
#endif
    if(submitted!=SAT_OK) {
        ++g_metrics.failures;
        ++g_metrics.animation_failed;
        if(sat_anim_decode(&skybridge_pig_anim_asset,&g_pig_anim,
                           g_pig_pose[write_buffer],PIG_VERTEX_CAP)!=SAT_OK) {
            ++g_metrics.failures;
            return;
        }
        g_pig_render_buffer=write_buffer;
        g_pig_render_anim=g_pig_anim;
        return;
    }
    ++g_metrics.animation_submitted;
    record_task_dispatch(slave_before,master_before,
        &g_metrics.animation_slave_dispatches,
        &g_metrics.animation_master_dispatches);
    g_pig_anim_pending=1u;
}

static void prepare_pig_animation_master(void) {
    const uint8_t write_buffer=(uint8_t)(1u-g_pig_render_buffer);
    sat_anim_state_t frame_state=g_pig_anim;
    if(sat_anim_decode(&skybridge_pig_anim_asset,&frame_state,
                       g_pig_pose[write_buffer],PIG_VERTEX_CAP)!=SAT_OK) {
        ++g_metrics.failures;
        return;
    }
    g_pig_render_buffer=write_buffer;
    g_pig_render_anim=frame_state;
}

static void finish_pig_animation(void) {
    const uint32_t start=sat_time_ms();
    if(!g_pig_anim_pending)return;
    if(g_parallel_recovery_blocked)return;
    {
        sat_result_t st=sat_parallel_wait(g_pig_anim_handle,SB_PARALLEL_TIMEOUT);
        sat_parallel_task_state_t state=sat_parallel_state(g_pig_anim_handle);
        if(st!=SAT_OK && state==SAT_PARALLEL_RUNNING) {
            ++g_metrics.timeouts;
            st=sat_parallel_abort(g_pig_anim_handle,SB_PARALLEL_TIMEOUT);
            state=sat_parallel_state(g_pig_anim_handle);
        }
        if(state==SAT_PARALLEL_RUNNING) {
            /* Neither timeout nor a failed abort returns ownership. Keep the
             * descriptor, state snapshot and output buffer live. */
            parallel_recovery_failed();
            return;
        }
        if(st==SAT_OK && state==SAT_PARALLEL_COMPLETED) {
            g_pig_render_buffer=g_pig_anim_write_buffer;
            g_pig_render_anim=g_pig_anim_job_state;
            ++g_metrics.animation_completed;
        } else {
            ++g_metrics.failures;
            ++g_metrics.animation_failed;
        }
#if SAT_SKYBRIDGE_VALIDATION
        const uint16_t release_start=sb_test_frt_counter();
#endif
        const sat_result_t released=sat_parallel_release(g_pig_anim_handle);
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.task_release_frt_ticks+=(uint16_t)(
            sb_test_frt_counter()-release_start);
#endif
        if(released!=SAT_OK) {
            parallel_recovery_failed();
            return;
        }
        g_pig_anim_pending=0u;
    }
    g_metrics.wait_ms += sat_time_ms()-start;
}

static uint8_t merge_gem_batch_direct(sat_scene3d_prepare_batch_t* batch) {
#if SAT_SKYBRIDGE_VALIDATION
    const uint16_t merge_start=sb_test_frt_counter();
#endif
    const uint16_t before=g_scene.faces.count;
    if(sat_scene3d_faces_merge_prepared(&g_scene.faces,batch)!=SAT_OK) {
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.geometry_merge_frt_ticks+=(uint16_t)(
            sb_test_frt_counter()-merge_start);
#endif
        ++g_metrics.failures;
        return 0u;
    }
    g_scene.submitted_faces=(uint16_t)(g_scene.submitted_faces+
                                       (g_scene.faces.count-before));
    g_scene.culled_faces=g_scene.faces.culled_faces;
    g_scene.clipped_faces=g_scene.faces.clipped_faces;
    g_metrics.prepared_faces=(uint16_t)(g_metrics.prepared_faces+
                                        batch->metrics.prepared_faces);
    record_gem_batch_merge(batch);
#if SAT_SKYBRIDGE_VALIDATION
    g_metrics.geometry_merge_frt_ticks+=(uint16_t)(
        sb_test_frt_counter()-merge_start);
#endif
    return 1u;
}

static void prepare_gem_geometry(const uint8_t* deck_slot) {
    uint8_t id;
    uint16_t count=0u;
    const uint32_t start=sat_time_ms();
#if SAT_SKYBRIDGE_VALIDATION
    const uint16_t prepare_start=sb_test_frt_counter();
#endif
    if(g_parallel_recovery_blocked)return;
    g_gem_master_ready=0u;
    g_gem_slave_ready=0u;
    g_gem_master_merged=0u;
    for(id=1u;id<=SB_PICKUP_COUNT;++id) {
        const sb_platform_t* p=&sb_course_platforms(&g_game)[id];
        sat_vec3_t center;
        sat_fx16_t depth;
        if(deck_slot[id]==SAT_FADE3D_SLOT_CULLED ||
           (g_game.pickups&(1u<<(id-1u))))continue;
        center=(sat_vec3_t){sb_platform_x(&g_game,id),
            sb_platform_surface_y(&g_game,id,sb_platform_x(&g_game,id),SB_F(p->z))+
            SB_GEM_BASE_OFFSET+sat_fx16_mul(
                sat_sin_deg(SB_F((int32_t)(g_game.ticks*5u+id*33u)%360)),
                SB_F(1)/2),SB_F(p->z)};
        if(sat_scene_depth(&g_scene,&center,&depth)!=SAT_OK || depth<=0 ||
           sb_abs(center.x-g_game.x)>SB_F(160) ||
           sb_abs(center.z-g_game.z)>SB_F(180))continue;
        if(sat_mat4_translate(&g_gem_world[id],center.x,center.y,center.z)!=SAT_OK) {
            ++g_metrics.failures; continue;
        }
        g_gem_instances[id].world=&g_gem_world[id];
        g_gem_batch_items[count]=(sat_scene3d_prepare_item_t){
            &g_gem_instances[id],g_gem_projected[id],g_gem_world_vertices[id],
            deck_slot[id],0u};
        ++count;
    }
    g_metrics.visible_gems=count;
    if(count==0u) {
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.geometry_prepare_frt_ticks+=(uint16_t)(
            sb_test_frt_counter()-prepare_start);
#endif
        return;
    }
    g_gem_partition_batch.items=g_gem_batch_items;
    g_gem_partition_batch.item_count=count;
    g_gem_partition_batch.view_proj=g_scene.faces.view_proj;
    g_gem_partition_batch.eye=g_scene.faces.eye;
    g_gem_partition_batch.forward=g_scene.faces.forward;
    g_gem_partition_batch.near_depth=g_scene.faces.near_depth;
    g_gem_partition_batch.width=g_scene.faces.width;
    g_gem_partition_batch.height=g_scene.faces.height;
    {
        uint16_t split=(uint16_t)(count/2u);
#if SAT_SKYBRIDGE_FORCE_GEM_SPLIT
        const uint8_t can_split=(uint8_t)(count>=4u);
#else
        const uint8_t can_split=(sat_parallel_mode()==SAT_PARALLEL_SLAVE &&
                                 sat_parallel_slave_available()!=0u && count>=4u);
#endif
        if(!can_split)split=count;
        g_metrics.master_gem_items=split;
        g_metrics.slave_gem_items=(uint16_t)(count-split);
        if(sat_scene3d_prepare_batch_slice(&g_gem_partition_batch,0u,split,
              g_gem_master_items,g_gem_master_faces,g_gem_master_keys,
              SB_GEM_BATCH_CAP,&g_gem_master_batch)!=SAT_OK) {
            ++g_metrics.failures; return;
        }
        if(split<count) {
            const uint32_t slave_before=parallel_slave_task_count();
            const uint32_t master_before=parallel_master_task_count();
            if(sat_scene3d_prepare_batch_slice(&g_gem_partition_batch,split,
                  (uint16_t)(count-split),g_gem_slave_items,g_gem_slave_faces,
                  g_gem_slave_keys,SB_GEM_BATCH_CAP,
                  &g_gem_slave_batch)!=SAT_OK) {
                ++g_metrics.failures; return;
            }
#if SAT_SKYBRIDGE_VALIDATION
            const uint16_t submit_start=sb_test_frt_counter();
#endif
#if SAT_SKYBRIDGE_FORCE_GEM_SPLIT
            /* Exercise the actual runtime-selected Slave/AUTO path without
             * compiling Skybridge-specific policy into the generic library. */
            g_gem_slave_batch.dispatch=SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME;
#endif
            const sat_result_t gem_submit=sat_scene_prepare_batch_async(
                &g_scene,&g_gem_slave_batch,&g_gem_slave_handle);
#if SAT_SKYBRIDGE_VALIDATION
            g_metrics.task_submit_frt_ticks+=(uint16_t)(
                sb_test_frt_counter()-submit_start);
#endif
#if SAT_SKYBRIDGE_VALIDATION
            g_metrics.task_input_publish_frt_ticks=
                sat_scene3d_test_input_publish_ticks();
#endif
            if(gem_submit!=SAT_OK) {
                ++g_metrics.failures;
                ++g_metrics.geometry_failed;
                /* Submission failed before a handle was accepted, so the
                 * Slave cannot own these buffers. Recover synchronously and
                 * publish readiness only after execution succeeds. */
                if(sat_scene3d_prepare_batch_execute(&g_gem_slave_batch)==SAT_OK) {
                    g_gem_slave_ready=1u;
                    g_metrics.gem_sync_fallback_items=(uint16_t)(count-split);
                }
            } else {
                ++g_metrics.geometry_submitted;
                record_task_dispatch(slave_before,master_before,
                    &g_metrics.geometry_slave_dispatches,
                    &g_metrics.geometry_master_dispatches);
                g_gem_slave_pending=1u;
            }
        }
        if(sat_scene3d_prepare_batch_execute(&g_gem_master_batch)!=SAT_OK) {
            ++g_metrics.failures;
            ++g_metrics.geometry_failed;
        } else {
            g_gem_master_ready=1u;
            g_metrics.master_gem_faces=g_gem_master_batch.metrics.prepared_faces;
        }
    }
#if SAT_SKYBRIDGE_VALIDATION
    g_metrics.geometry_prepare_frt_ticks+=(uint16_t)(
        sb_test_frt_counter()-prepare_start);
#endif
    g_metrics.geometry_ms += sat_time_ms()-start;
}

static void finish_gem_geometry(void) {
    if(g_gem_master_ready) {
        g_gem_master_merged=merge_gem_batch_direct(&g_gem_master_batch);
        g_gem_master_ready=0u;
    }
    if(g_gem_slave_ready && g_gem_master_merged) {
        merge_gem_batch_direct(&g_gem_slave_batch);
        g_gem_slave_ready=0u;
    }
    if(g_gem_slave_ready && !g_gem_master_merged) {
        ++g_metrics.failures;
        g_gem_slave_ready=0u;
    }
    if(!g_gem_slave_pending)return;
    if(g_parallel_recovery_blocked)return;
    {
        const uint32_t start=sat_time_ms();
        uint8_t sync_fallback_ready=0u;
        sat_result_t st=sat_parallel_wait(g_gem_slave_handle,SB_GEM_WAIT_TIMEOUT);
        sat_parallel_task_state_t state=sat_parallel_state(g_gem_slave_handle);
        g_metrics.gem_task_state=(uint16_t)state;
        if(st!=SAT_OK && state==SAT_PARALLEL_RUNNING) {
            ++g_metrics.timeouts;
            st=sat_parallel_abort(g_gem_slave_handle,SB_PARALLEL_TIMEOUT);
            state=sat_parallel_state(g_gem_slave_handle);
            g_metrics.gem_task_state=(uint16_t)state;
        }
        if(state==SAT_PARALLEL_RUNNING) {
            parallel_recovery_failed();
            return;
        }
        if(st==SAT_OK && state==SAT_PARALLEL_COMPLETED) {
            ++g_metrics.geometry_completed;
            g_metrics.slave_gem_faces=g_gem_slave_batch.metrics.prepared_faces;
#if SAT_SKYBRIDGE_VALIDATION
            const uint16_t merge_start=sb_test_frt_counter();
#endif
            const sat_result_t merged=(!g_gem_master_merged)?SAT_ERR_BUSY:
                sat_scene_merge_prepared_batch(&g_scene,&g_gem_slave_batch,
                                               g_gem_slave_handle);
#if SAT_SKYBRIDGE_VALIDATION
            g_metrics.geometry_merge_frt_ticks+=(uint16_t)(
                sb_test_frt_counter()-merge_start);
#endif
            if(merged!=SAT_OK) {
                ++g_metrics.failures;
                ++g_metrics.geometry_failed;
            } else {
                g_metrics.prepared_faces=(uint16_t)(g_metrics.prepared_faces+
                    g_gem_slave_batch.metrics.prepared_faces);
                record_gem_batch_merge(&g_gem_slave_batch);
            }
        } else {
            ++g_metrics.failures;
            ++g_metrics.geometry_failed;
            /* A terminal worker error/aborted task no longer owns the
             * buffers. Recompute its suffix on the Master, but never do so
             * while a timed-out worker remains active. */
            if(state==SAT_PARALLEL_FAILED && g_gem_master_merged &&
               sat_scene3d_prepare_batch_execute(&g_gem_slave_batch)==SAT_OK) {
                sync_fallback_ready=1u;
                g_metrics.gem_sync_fallback_items=
                    g_gem_slave_batch.item_count;
            }
        }
#if SAT_SKYBRIDGE_VALIDATION
        const uint16_t release_start=sb_test_frt_counter();
#endif
        const sat_result_t released=sat_scene_prepare_batch_release(
            &g_scene,&g_gem_slave_batch,g_gem_slave_handle);
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.task_release_frt_ticks+=(uint16_t)(
            sb_test_frt_counter()-release_start);
#endif
        if(released!=SAT_OK) {
            parallel_recovery_failed();
            return;
        }
        g_gem_slave_pending=0u;
        if(sync_fallback_ready) {
            merge_gem_batch_direct(&g_gem_slave_batch);
        }
        g_metrics.wait_ms += sat_time_ms()-start;
    }
}
/* The VDP2 fade setup intentionally makes only indexed Sprite Type 0
 * materials opaque at priority 7. The converter's RGB shade palette is
 * therefore uploaded into the spare entries of the fade bank and each
 * animated face selects a tiny indexed solid texture. This keeps the pig
 * opaque and pink instead of blending it with the RBG0 sea. */
static void player_pig(void) {
    if(g_world_cmd_full) return;
    sat_model_transform3d_t pose;
    sat_mat4_t world;
    sat_model_transform3d_identity(&pose);
    pose.position=(sat_vec3_t){g_game.x,
        g_game.y-SB_F(1)/2,g_game.z};
    /* glTF local +Z points towards the snout. The game controller stores
     * a persistent CARDINAL forward independent of camera yaw. */
    pose.rotation_deg.y=g_game.facing_z<0?SB_F(180):
        (g_game.facing_x>0?SB_F(90):
         (g_game.facing_x<0?SB_F(270):0));
    sat_example_must(sat_model_transform3d_matrix(&pose,&world));
    /* The Slave only decodes into the alternate local-pose buffer. The
     * Master owns this mesh and applies the world transform after completion,
     * so no transformed vertex range is concurrently recycled. */
    for(uint16_t vertex=0u;vertex<SKYBRIDGE_PIG_VERTEX_COUNT;++vertex)
        g_pig_mesh.vertices[vertex]=g_pig_pose[g_pig_render_buffer][vertex];
    if(sat_mesh_transform(&g_pig_mesh,&world)!=SAT_OK ||
       sat_anim_face_colors(&skybridge_pig_anim_asset,
        &g_pig_render_anim,g_pig_face_textures,PIG_FACE_CAP)!=SAT_OK) {
        ++g_metrics.failures;
        return;
    }
    /* Animated pose already carries the world transform; do not apply it
     * twice when submitting the pig through the canonical instance path. */
    g_pig_instance.mesh=&g_pig_mesh;
    g_pig_instance.materials=g_pig_materials;
    g_pig_instance.material_count=SKYBRIDGE_PIG_SHADE_COUNT;
    g_pig_instance.face_materials=g_pig_face_textures;
    g_pig_instance.world=0;
    g_pig_instance.pass=SB_PASS_ACTOR;
    g_pig_instance.cull_backfaces=1u;
    /* Deliberately SLOT_INHERIT, not a fade slot: the player's character is
     * the one thing that must stay readable at any camera distance. The gems
     * around it do fade, through the same parameter. */
    {
        const uint16_t before=g_scene.faces.count;
        world_ok(sat_scene_submit_instance(
            &g_scene,&g_pig_instance,SAT_SCENE3D_SLOT_INHERIT,g_pig_projected,0));
        g_dbg_pig_faces=(uint16_t)(g_scene.faces.count-before);
    }
}
/* The one fade rule that really is the GAME's: whatever the distance says,
 * the deck under the player's feet stays fully readable. Quantization, the
 * level-to-slot mapping and the anti-flicker band are the library's, and
 * g_platform_fade[id] is the per-deck byte sat_fade3d_slot keeps them in. */
static uint8_t platform_fade_slot(uint8_t id,int32_t depth) {
    uint8_t slot=SAT_INDEXED_SOLID_OPAQUE;
    if(g_game.support==(int8_t)id) {
        g_platform_fade[id]=SAT_INDEXED_SOLID_OPAQUE;
        return SAT_INDEXED_SOLID_OPAQUE;
    }
    if(sat_fade3d_slot(&g_fade_slots,depth,
                       &g_platform_fade[id],&slot)!=SAT_OK)
        return SAT_FADE3D_SLOT_CULLED;
    return slot;
}
static void draw_world(void) {
    /* Each deck's chosen slot, doubling as the visibility flag: CULLED means
     * the deck was not drawn this frame. Anything sitting ON a deck -- its
     * gem, the player's contact shadow -- has to fade WITH it, so the slot
     * has to outlive the loop that picked it. */
    uint8_t deck_slot[SB_PLATFORM_COUNT];
    uint8_t i;
    for(i=0u;i<SB_PLATFORM_COUNT;++i)deck_slot[i]=SAT_FADE3D_SLOT_CULLED;
    g_dbg_scene_status=sat_scene_begin(
        &g_scene,&g_camera,SB_F(8),W,H,SB_HUD_COMMAND_RESERVE);
    if(g_dbg_scene_status!=SAT_OK) {
        sat_vdp1_command_stats_t stats={0};
        sat_vdp1_command_stats(&stats);
        put_text("SCENE BEGIN ERROR",48,84);
        label("ERR",(uint32_t)(-g_dbg_scene_status),48,100);
        label("CMD",stats.used,48,116);
        label("CAP",stats.capacity,48,132);
        label("RES",stats.overlay_reserved,48,148);
        return;
    }
    g_active_pass=SB_PASS_WORLD;

    /* All visible faces share render pass zero, independent of which object
     * contributed them. Gameplay visibility/fade still belongs to the game. */
    for(i=0u;i<SB_PLATFORM_COUNT;++i) {
        const sb_platform_t* p=&sb_course_platforms(&g_game)[i];
        sat_vec3_t center;
        sat_fx16_t depth;
        uint8_t slot;
        if(!sb_platform_active(&g_game,i))continue;
        center=(sat_vec3_t){
            sb_platform_x(&g_game,i),
            sb_platform_surface_y(&g_game,i,sb_platform_x(&g_game,i),SB_F(p->z)),
            SB_F(p->z)
        };
        /* Camera-penetration guard for an already-passed platform. At the
         * reported C1 position X~7 Z~30, the chase camera (42 units behind
         * the pig) sits inside the previous pier's XZ footprint. Drawing
         * the pier right around the camera adds several nearly full-screen
         * VDP1 raster commands, even after geometric clipping. That pier
         * is NOT the supporting deck; omit it while the eye is inside its
         * footprint, without touching world/collision/collectibles.
         * Apply to ALL courses and decks; no hardcoded stage index. */
        if(g_game.support!=(int8_t)i &&
           g_eye.y>center.y+SB_F(6) &&
           sb_abs(g_eye.x-center.x)<SB_F(p->half_x) &&
           sb_abs(g_eye.z-center.z)<SB_F(p->half_z))
            continue;
        sat_example_must(sat_scene_depth(&g_scene,&center,&depth));
        if(g_game.support!=(int8_t)i &&
           (depth < -SB_F(9) ||
            sb_abs(center.x-g_game.x)>SB_F(160) ||
            sb_abs(center.z-g_game.z)>SB_F(180)))continue;
        slot=platform_fade_slot(i,depth);
        if(slot==SAT_FADE3D_SLOT_CULLED)continue;
        g_active_fade_slot=slot;
        g_active_pass=(g_game.support==(int8_t)i)?SB_PASS_SUPPORT:SB_PASS_WORLD;
        stage_box(i);
        deck_slot[i]=slot;
    }
    g_active_fade_slot=SAT_INDEXED_SOLID_OPAQUE;
    g_active_pass=SB_PASS_ACTOR;
    /* Gema instances are independent of the pig pose. Dispatch their
     * suffix before the Master finishes the actor preparation. */
    prepare_gem_geometry(deck_slot);
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT
    /* Keep the injected timeout adjacent to submission, while its real Slave
     * worker is known to be active. Production and other validation profiles
     * retain the normal frame schedule below. */
    finish_gem_geometry();
#endif
    /* The pig must advance on every gameplay frame. If the Slave owns the gem
     * suffix, decode this frame's immutable animation snapshot on the Master;
     * otherwise the Slave may decode it asynchronously. */
    if(g_gem_slave_pending) prepare_pig_animation_master();
    else submit_pig_animation();
    finish_pig_animation();
    pig_shadow(deck_slot);
    player_pig();
    finish_gem_geometry();
    /* World, pig and gem faces are ordered together before the protected HUD. */
    g_dbg_faces=g_scene.faces.count;
    g_dbg_face_cap=g_scene.faces.capacity;
    if(sat_scene_flush(&g_scene)!=SAT_OK) {
        ++g_metrics.failures;
        g_dbg_scene_status=SAT_ERR_VERIFY_FAILED;
    }
    {
        /* AFTER the flush: that is where the queued faces actually become
         * VDP1 commands, so sampling before it always reported an empty list
         * and hid exactly the exhaustion this row exists to show. Still before
         * the HUD pass, which spends its own reserved quota. */
        sat_vdp1_command_stats_t cmd={0};
        if(sat_vdp1_command_stats(&cmd)==SAT_OK) {
            g_dbg_cmds=cmd.used;
            g_dbg_cmd_cap=(uint16_t)(cmd.capacity-cmd.overlay_reserved);
        }
        g_dbg_world_full=g_world_cmd_full;
    }
}
static void init_tile_texture(void) {
    static const uint8_t banks[3]={3u,5u,6u};
    static const uint8_t colors[3][3][3]={
        {{9u,23u,18u},{17u,28u,22u},{26u,30u,27u}},
        {{8u,18u,24u},{13u,25u,28u},{23u,29u,30u}},
        {{21u,17u,9u},{27u,23u,14u},{31u,29u,22u}}
    };
    uint16_t palette[256];
    uint16_t x,y;
    uint8_t theme;
    /* Broad 4x4 paving motifs, a quiet border and sparse highlights:
     * distinguish the three regions without a noisy repeating checker. */
    for (y=0u;y<16u;++y) for (x=0u;x<16u;++x) {
        uint8_t cell=(uint8_t)(((x>>2u)+(y>>2u))&1u);
        uint8_t grout=(uint8_t)((x&7u)==0u || (y&7u)==0u);
        uint8_t glint=(uint8_t)((x==5u && y==4u)||(x==13u && y==12u));
        g_tile_pixels[y*16u+x]=glint?3u:(grout?2u:(uint8_t)(1u+cell));
    }
    for(theme=0u;theme<3u;++theme) {
        for(x=0u;x<256u;++x)palette[x]=SAT_BGR555(0,0,0);
        for(x=0u;x<3u;++x)
            palette[x+1u]=SAT_RGB555(colors[theme][x][0],
                                     colors[theme][x][1],
                                     colors[theme][x][2]);
        sat_example_must(sat_tex_upload_indexed8(
            &g_tile_textures[theme],g_tile_pixels,16u,16u,palette,banks[theme]));
        /* The renderer's asset preparation owns source-region packing.
         * No example-local crop/stride logic, and no per-frame VRAM writes. */
        sat_example_must(sat_upload_indexed8_quadrants(
            g_tile_pixels,16u,16u,16u,banks[theme],
            g_tile_quadrants[theme],g_tile_quadrant_pixels,
            sizeof(g_tile_quadrant_pixels)));
        g_tile_regions[theme].full=&g_tile_textures[theme];
        for(uint8_t tile=0u;tile<4u;++tile)
            g_tile_regions[theme].tiles[tile]=&g_tile_quadrants[theme][tile];
    }
}
static void init_cloud_texture(void) {
    uint16_t palette[256];
    uint16_t x,y;
    for(x=0u;x<256u;++x)palette[x]=SAT_BGR555(0,0,0);
    palette[1u]=SAT_RGB555(31,31,31);
    palette[2u]=SAT_RGB555(28,30,31);
    palette[3u]=SAT_RGB555(23,27,30);
    palette[4u]=SAT_RGB555(18,24,28);
    for(y=0u;y<SB_CLOUD_H;++y)for(x=0u;x<SB_CLOUD_W;++x)
        g_cloud_pixels[y*SB_CLOUD_W+x]=sb_scenery_cloud_pixel(x,y);
    /* CRAM bank 7 is deliberately separate from sea 0, sky 1,
     * font 2, stage 3/5/6 and the fade-material bank 4. */
    sat_example_must(sat_tex_upload_indexed8(
        &g_cloud_texture,g_cloud_pixels,SB_CLOUD_W,SB_CLOUD_H,palette,7u));
}
static void draw_clouds(void) {
    static const uint16_t base_x[6]={18u,102u,206u,315u,405u,488u};
    static const uint8_t y[6]={21u,42u,27u,15u,48u,32u};
    static const uint8_t width[6]={68u,52u,79u,60u,55u,72u};
    static const uint8_t height[6]={15u,12u,18u,14u,12u,16u};
    uint8_t i,copy;
#if SAT_SKYBRIDGE_VALIDATION
    /* Clouds are world VDP1 commands, so the cross-profile scene hash would
     * otherwise compare display-frame pacing instead of rendering: drive
     * them from the gameplay tick every profile shares. */
    const uint32_t cloud_clock=g_game.ticks;
#else
    const uint32_t cloud_clock=g_frame;
#endif
    for(i=0u;i<6u;++i) {
        const int32_t x=sb_scenery_cloud_x(base_x[i],(uint16_t)g_yaw,cloud_clock);
        /* Each cloud is drawn at its own position and once more one full
         * 512px period to the left, so a cloud straddling the seam stays
         * whole. Wrapping every cloud at 320px instead would visibly tile
         * the sky. */
        for(copy=0u;copy<2u;++copy) {
            const int32_t cx=x-(int32_t)copy*512;
            const int32_t half=(int32_t)width[i]/2;
            if(cx+half<=0 || cx-half>=(int32_t)W || g_world_cmd_full)
                continue;
            world_ok(sat_draw_sprite_scaled_screen(
                &g_cloud_texture,(int16_t)cx,(int16_t)y[i],
                width[i],height[i],0u));
        }
    }
}
static void init_scene_materials(void) {
    sat_example_must(sat_scene3d_solid_pool_init(
        &g_solid_pool,g_scene_materials,g_solid_textures,
        g_solid_colors,g_solid_pixels,SCENE_MATERIAL_CAP,FADE_PALETTE_BANK));
    /* Fade colours are registered first; world/gem material selectors share
     * the same handles and every colour is uploaded once, regardless of
     * how many platform faces, gems or pig shades reference it. */
    for(uint16_t i=0u;i<SB_WORLD_COLOR_COUNT;++i) {
        uint16_t handle=0u;
        sat_example_must(sat_scene3d_solid_pool_register(
            &g_solid_pool,g_world_colors[i],&handle));
        /* Registration order IS the SB_C_* order, so a palette name can be
         * used directly as a material index. The pool deduplicates, so two
         * identical colours in the list would silently shift every later
         * name; fail loudly instead. */
        sat_example_must(handle==i?SAT_OK:SAT_ERR_INVALID_ARG);
    }
    for(uint16_t i=0u;i<SKYBRIDGE_PIG_SHADE_COUNT;++i) {
        uint16_t handle=0u;
        sat_example_must(sat_scene3d_solid_pool_register(
            &g_solid_pool,skybridge_pig_shade_palette[i],&handle));
        g_pig_materials[i]=g_scene_materials[handle];
    }
    sat_example_must(sat_scene3d_solid_pool_upload_palette(&g_solid_pool));
}
static void init_sky(void) {
    uint32_t x,y;
    for(x=0u;x<256u;++x)g_sky_colors[x]=sb_scenery_sky_color(x);
    for(y=0u;y<SKY_H;++y)for(x=0u;x<SKY_W;++x)
        g_sky[y*SKY_W+x]=sb_scenery_sky_pixel(x,y);
}
static uint8_t ocean_bitmap_pixel(void* user,uint16_t x,uint16_t y) {
    (void)user;
    return sb_scenery_sea_pixel(x,y);
}
static void ocean_bitmap_progress(void* user,uint16_t rows_complete) {
    (void)user;
    if((rows_complete&15u)==0u)
        loading_frame("GENERATING OCEAN",
            (uint8_t)(15u+((uint32_t)rows_complete*60u/SB_SEA_H)));
}
static void init_sea(void) {
    uint16_t row_words[SB_SEA_W/2u];
    for(uint32_t x=0u;x<256u;++x)
        g_sea_colors[x]=sb_scenery_sea_color(x&63u);
    sat_example_must(sat_vdp2_bitmap_upload_indexed8(
        SEA_WORD,SB_SEA_W,SB_SEA_H,
        ocean_bitmap_pixel,ocean_bitmap_progress,0,
        row_words,SB_SEA_W/2u));
}
static void animate_sea_palette(uint32_t tick) {
    /* Palette modulation touches only eight highlight colors: 16 bytes
     * every 8 display frames, never a 128 KiB bitmap or the fade registers.
     * The RBG0 scroll supplies flow and this supplies subtle foam shimmer. */
    uint32_t phase=(tick>>3u)&31u;
    uint32_t light=phase<16u?phase:31u-phase;
    uint32_t i;
    if(g_last_ocean_palette_step==phase)return;
    g_last_ocean_palette_step=phase;
    for(i=0u;i<8u;++i) {
        uint32_t t=i+(light>>2u);
        g_sea_colors[48u+i]=sb_scenery_rgb(6u+t/3u,18u+t/2u,
                                           24u+t/4u);
    }
    sat_example_must(sat_vdp2_palette_upload(
        &g_sea_colors[48u],8u,48u));
}
static void update_rotation(int32_t fx,int32_t fz) {
    uint16_t* p = g_environment_params;
    /* Keep the sea under a fixed 96px horizon, rotate/scroll sample plane. */
    /* Two slow, non-identical currents slide the textured water plane
     * underneath a fixed world horizon without any per-frame bitmap upload. */
    sat_example_must(sat_vdp2_ground_environment_build_params(&g_environment,
        (g_game.x>>16)+(int32_t)(g_frame/9u),
        (g_game.z>>16)+(int32_t)(g_frame/17u)));
    p[15]=(uint16_t)((uint32_t)fz&0xFFFFu);
    p[17]=(uint16_t)((uint32_t)fx&0xFFFFu);
    p[21]=(uint16_t)((uint32_t)(-fx)&0xFFFFu);
    p[23]=(uint16_t)((uint32_t)fz&0xFFFFu);
    /* The parameter table uses signed 16.16 A/B/D/E, including high words. */
    p[14]=(uint16_t)(fz>>16);
    p[16]=(uint16_t)(fx>>16);
    p[20]=(uint16_t)((-fx)>>16);
    p[22]=(uint16_t)(fz>>16);
    sat_example_must(sat_vdp2_ground_environment_commit_params(&g_environment));
}
static void init_background(void) {
    const sat_vdp2_nbg0_config_t sky = {
        SAT_VDP2_CHAR_SIZE_1X1,SAT_VDP2_COLOR_MODE_256,0x3Bu,0u,0u
    };
    const sat_vdp2_rbg0_mode7_config_t sea = {
        SAT_VDP2_RBG0_BITMAP_512x256,SAT_VDP2_COLOR_MODE_256,
        SEA_WORD,ROT_WORD,SAT_COLOR_BLACK,5u,7u
    };
    sat_example_must(sat_vdp2_ground_environment_validate_layout(
        &g_ocean, &sea, H, SAT_VDP2_VRAM_WORD_CAPACITY, 0u, 512u, 2u));
    sat_example_must(sat_vdp2_ground_environment_init(&g_environment,
        &g_ocean, &sea, g_environment_coefficients, H * 2u,
        g_environment_params, H));
    sat_example_must(sat_vdp2_palette_upload(g_sea_colors,256u,0u));
    sat_example_must(sat_vdp2_palette_upload(g_sky_colors,256u,256u));
    sat_example_must(sat_vdp2_nbg0_init(&sky));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(g_sky,SKY_W,SKY_H,1u,g_map));
    sat_example_must(sat_vdp2_ground_environment_upload_coefficients(&g_environment));
    sat_example_must(sat_vdp2_ground_environment_build_params(&g_environment,0,0));
    sat_example_must(sat_vdp2_ground_environment_commit_params(&g_environment));
    sat_example_must(sat_vdp2_nbg0_set_priority(2u));
    sat_example_must(sat_vdp2_rbg0_set_priority(5u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
    sat_example_must(sat_vdp2_sprite_color_calc_configure_alpha(7u));
    loading_frame("COMPOSING VDP2",82u);
}
static void init_audio(void) {
    uint32_t i;
    uint8_t j;
    const uint8_t tone_step[SOUNDS-1u]={3u,5u,7u,2u,9u};
    sat_sound_play_params_t music_params={76u,0,1u,0u,SB_F(1)};
    sat_example_must(sat_audio_init());
    sat_example_must(sat_audio_set_master_volume(160u));
    for(j=0u;j<SOUNDS-1u;++j) {
        /* The fall cue is the odd one out: noise, not a tone. */
        sat_example_must(j==3u
            ? sat_audio_synth_noise(g_audio[j],SOUND_LEN)
            : sat_audio_synth_blip(g_audio[j],SOUND_LEN,tone_step[j]));
        {
            sat_asset_desc_t asset={0};
            sat_asset_t asset_handle={0};
            asset.logical_path=g_sfx_paths[j];
            asset.kind=SAT_ASSET_SOUND;
            asset.data=g_audio[j];
            asset.size=SOUND_LEN;
            asset.sample_rate=11025u;
            asset.sample_count=SOUND_LEN;
            asset.channels=1u;
            asset.format=SAT_AUDIO_PCM_S8;
            sat_example_must(sat_asset_register(&asset,&asset_handle));
            sat_example_must(sat_sound_load(g_sfx_paths[j],&g_sounds[j]));
        }
        loading_frame("REGISTERING AUDIO",(uint8_t)(82u+(j+1u)*2u));
    }
    {
        /* 8-note, three-second arpeggio, one note per call so the boot panel
         * can report progress between them. */
        static const uint8_t periods[8]={50u,45u,40u,38u,34u,38u,40u,45u};
        const uint32_t note_len=MUSIC_LEN/8u;
        for(i=0u;i<8u;++i) {
            sat_example_must(sat_audio_synth_arpeggio_note(
                &g_music[i*note_len],note_len,periods[i],i*note_len));
            loading_frame("GENERATING MUSIC",
                (uint8_t)(92u+(i+1u)*6u/8u));
        }
    }
    {
        sat_sound_desc_t desc={0};
        desc.samples=g_music;desc.sample_count=MUSIC_LEN;
        desc.sample_rate=11025u;desc.format=SAT_AUDIO_PCM_S8;
        desc.loop=1u;
        sat_example_must(sat_sound_create(&g_sounds[5u],&desc));
        sat_example_must(sat_sound_play(g_sounds[5u],&music_params,&g_music_voice));
    }
    g_audio_ready=1u;
    loading_frame("READY",100u);
}
static void sound_event(uint16_t event) {
    uint8_t id;
    sat_sound_play_params_t p={96u,0,8u,0u,SB_F(1)};
    if (!g_audio_ready || !event) return;
    if (event&SB_EVENT_WIN) id=4u;
    else if (event&SB_EVENT_FALL) id=3u;
    else if (event&SB_EVENT_CHECKPOINT) id=2u;
    else if (event&SB_EVENT_PICKUP) id=2u;
    else if (event&SB_EVENT_JUMP) id=0u;
    else if (event&SB_EVENT_LAND) id=1u;
    else if (event&SB_EVENT_WARNING) id=1u;
    else return;
    (void)sat_sound_play(g_sounds[id],&p,0);
}
static void start_course(uint8_t course) {
    uint8_t i;
    sb_start_course(&g_game,course);
    g_yaw=0;
    for(i=0u;i<SB_PLATFORM_COUNT;++i)g_platform_fade[i]=SAT_INDEXED_SOLID_OPAQUE;
    sat_example_must(sat_follow_camera3d_init(
        &g_follow_camera,&(sat_vec3_t){g_game.x,g_game.y,g_game.z},4u,6u,4u));
    sat_example_must(sat_anim_state_init(
        &g_pig_anim,&skybridge_pig_anim_asset,1u));
}
/* Per-course HUD strings are tables indexed by the course, not three
 * separate ternary ladders that have to be kept in the same order. */
static const char* const k_course_complete[4]={
    "COURSE 1 COMPLETE","COURSE 2 COMPLETE",
    "COURSE 3 COMPLETE","COURSE 4 COMPLETE"};
static const char* const k_course_next[4]={
    "START: COURSE 2","START: COURSE 3",
    "START: COURSE 4","START: REPLAY"};
static const char* const k_course_hint[4]={
    "GEMS OPTIONAL  Z BRAKE","GEMS OPTIONAL  Z BRAKE",
    "JUMP THE YELLOW-RIM HOLES","RIDE THE TILTING RAMPS"};
static void hud(void) {
    uint8_t i,count=0u;
    for(i=0u;i<SB_PICKUP_COUNT;++i) if(g_game.pickups&(1u<<i)) ++count;
    (void)sat_draw_rect_screen(0,0,W,16u,SAT_RGB555(3,8,15));
    put_text("SKYBRIDGE",7,4);
    label("C",g_game.course+1u,104,4);
    label("GEMS ",count,144,4);
    put_text("/8",192,4);
    label("TIME ",g_game.ticks/60u,224,4);
    /* Always show player position, not the smoothed camera anchor. */
    (void)sat_draw_rect_screen(0,17,W,13u,SAT_RGB555(3,8,15));
    hud_coord('X',g_game.x,7,19);
    hud_coord('Y',g_game.y,112,19);
    hud_coord('Z',g_game.z,217,19);
    if(g_show_debug) {
        /* Y toggles the extra camera-orbit diagnostic below the XYZ row. */
        uint8_t surface=sb_ground_surface(&g_game,g_game.support);
        (void)sat_draw_rect_screen(0,31,W,13u,SAT_RGB555(3,8,15));
        label("DECK ",g_game.support<0?0u:(uint32_t)g_game.support+1u,7,33);
        label("YAW ",(uint32_t)g_yaw,101,33);
        if(g_game.support>=0 &&
           sb_course_platforms(&g_game)[(uint8_t)g_game.support].kind==SB_SEESAW)
            hud_coord('T',g_game.seesaw_tilt[(uint8_t)g_game.support],197,33);
        else
            put_text(surface==SB_SURFACE_SLICK?"ICE":
                     surface==SB_SURFACE_GRIP?"GRIP":
                     surface==SB_SURFACE_AIR?"AIR":"NORMAL",197,33);
        /* Budget row: the two limits that silently eat geometry. F is the
         * painter queue, C the VDP1 command list, P how many of the character's
         * faces actually reached the queue. Either budget at its cap explains a
         * half-drawn frame; all three well under it means the cause is paint
         * ORDER, not budget. The library lays the cells out, so adding a field
         * cannot make two of them overlap into an unreadable number. */
        const sat_debug_field_t budget[3]={
            {"F ",g_dbg_faces,g_dbg_face_cap},
            {"C ",g_dbg_cmds,g_dbg_cmd_cap},
            {"P ",g_dbg_pig_faces,SKYBRIDGE_PIG_FACE_COUNT}};
        (void)sat_draw_rect_screen(0,45,W,13u,SAT_RGB555(3,8,15));
        (void)sat_ascii_font_draw_fields(
            &g_font,budget,3u,7,47,W-14u,8,0u,0u);
        if(g_dbg_world_full) put_text("FULL",270,47);
        (void)sat_draw_rect_screen(0,59,W,13u,SAT_RGB555(3,8,15));
        label("MS ",g_metrics.frame_cpu_ms,7,61);
        label("WAIT ",g_metrics.wait_ms,66,61);
        label("TASK ",g_metrics.task_count,151,61);
        label("ERR ",g_metrics.failures,244,61);
    }
    if (g_game.finished) {
        (void)sat_draw_rect_screen(46,76,228u,75u,SAT_RGB555(2,13,16));
        put_text(k_course_complete[g_game.course&3u],66,83);
        label("GEMS ",count,116,104);
        put_text("/8",164,104);
        put_text(k_course_next[g_game.course&3u],82,128);
    } else if (g_game.paused) {
        put_text("START: PLAY COURSE",80,92);
        put_text("X: NEXT COURSE",88,108);
    } else if(g_show_help && g_game.ticks<480u) {
        put_text(k_course_hint[g_game.course&3u],8,192);
        put_text("D-PAD MOVE  A JUMP",8,204);
        put_text("B/C CAMERA  START PAUSE",8,215);
    }
}

static void plan_resources(void) {
    sat_example_must(sat_resource_plan_init(
        &g_resources, g_resource_entries,
        (uint16_t)(sizeof(g_resource_entries) / sizeof(g_resource_entries[0]))));
    /* Fixed reservations are checked before uploads and scene activation. */
    sat_example_must(sat_resource_plan_set_limit(
        &g_resources, SAT_RESOURCE_WRAM, sizeof(g_face_items) +
        sizeof(g_face_keys) + sizeof(g_face_order)));
    sat_example_must(sat_resource_plan_set_limit(
        &g_resources, SAT_RESOURCE_VDP1_COMMANDS, 1024u * 4u));
    sat_example_must(sat_resource_plan_set_limit(
        &g_resources, SAT_RESOURCE_AUDIO_STAGING,
        SOUNDS * SOUND_LEN + MUSIC_LEN));
    sat_example_must(sat_resource_plan_add(
        &g_resources, SAT_RESOURCE_WRAM,
        sizeof(g_face_items) + sizeof(g_face_keys) + sizeof(g_face_order),
        2u, 1u));
    sat_example_must(sat_resource_plan_add(
        &g_resources, SAT_RESOURCE_VDP1_COMMANDS, 1024u * 4u, 4u, 1u));
    sat_example_must(sat_resource_plan_add(
        &g_resources, SAT_RESOURCE_AUDIO_STAGING,
        SOUNDS * SOUND_LEN + MUSIC_LEN, 2u, 1u));
    sat_example_must(sat_resource_plan_finalize(&g_resources));
}

int main(void) {
    const sat_video_config_t video={W,H,1u,0u};
    const sat_vec3_t up={0,SB_F(1),0};
    const sat_vec3_t g_game_origin={0,0,0};
    const sat_vec3_t g_game_origin_ahead={0,0,SB_F(1)};
    sat_pad_state_t pad={0};
    sat_vdp2_scroll_t sky_scroll={0u,0u,31u,0u};
#if SAT_SKYBRIDGE_VALIDATION
    initialize_test_telemetry();
#endif
    /* Keep immutable asset preparation ahead of SSHON. */
    {
        const sat_vec3_t origin={0,0,0};
        sat_example_must(sat_mesh_init(&g_gem_mesh,g_gem_vertices,GEM_VERTEX_CAP,
                                      g_gem_indices,GEM_FACE_CAP));
        sat_example_must(sat_mesh_build_octahedron(&g_gem_mesh,&origin,
                         SB_GEM_RADIUS,SB_GEM_HALF_HEIGHT));
        for(uint8_t face=0u;face<GEM_FACE_CAP;++face)
            g_gem_materials[face]=g_gem_facet_colors[face];
    }
    sat_example_must(sat_model_validate(&skybridge_pig_asset));
    sat_example_must(sat_anim_validate(&skybridge_pig_anim_asset));
    sat_example_must(sat_mesh_init(&g_pig_mesh,
        g_pig_vertices,PIG_VERTEX_CAP,g_pig_indices,PIG_FACE_CAP));
    sat_example_must(sat_model_copy_to_mesh(&skybridge_pig_asset,&g_pig_mesh));
    sat_example_must(sat_anim_state_init(
        &g_pig_anim,&skybridge_pig_anim_asset,1u)); /* Idle */
    {
        sat_mat4_t identity={0};
        identity.m[0]=identity.m[5]=identity.m[10]=identity.m[15]=SAT_FX16_ONE;
        sat_example_must(sat_anim_prepare_model_instance(
            &skybridge_pig_anim_asset,&g_pig_anim,&identity,&g_pig_mesh,
            g_pig_face_textures,PIG_FACE_CAP,SKYBRIDGE_PIG_SHADE_COUNT));
    }
    g_pig_render_anim=g_pig_anim;
    sat_example_must(sat_anim_decode(&skybridge_pig_anim_asset,&g_pig_anim,
                                    g_pig_pose[0],PIG_VERTEX_CAP));
    g_pig_render_buffer=0u;
    sat_example_must(sat_scene3d_prepare_batch_init(
        &g_gem_partition_batch,g_gem_batch_items,0u,
        g_gem_partition_faces,g_gem_partition_keys,
        SB_GEM_BATCH_CAP));
    sat_example_must(sat_init(&video));
    {
        const sat_parallel_config_t parallel_config={
            (sat_parallel_mode_t)SAT_SKYBRIDGE_PARALLEL_MODE,0,0u,0u,
            SB_PARALLEL_TIMEOUT};
        sat_example_must(sat_anim_parallel_register());
        sat_example_must(sat_scene3d_prepare_parallel_register());
        sat_example_must(sat_parallel_init(&parallel_config));
    }
    plan_resources();
    sb_init(&g_game);
    sat_example_must(sat_camera3d_init(
        &g_camera,&g_game_origin,&g_game_origin_ahead,&up,SB_F(55),
        sat_fx16_div(SB_F(W),SB_F(H)),SB_F(2),SB_F(250)));
    sat_example_must(sat_scene_init(
        &g_scene,g_face_items,g_face_keys,g_face_order,SCENE_FACE_CAP));
    for(uint8_t i=0u;i<SB_PLATFORM_COUNT;++i)
        g_platform_fade[i]=SAT_INDEXED_SOLID_OPAQUE;
    sat_example_must(sat_follow_camera3d_init(
        &g_follow_camera,&(sat_vec3_t){g_game.x,g_game.y,g_game.z},4u,6u,4u));
    /* Load the first drawable font before expensive procedural generation. */
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,2u));
    sat_example_must(sat_vdp1_set_erase_transparent());
    loading_frame("INITIALIZING WORLD",5u);
    init_tile_texture();
    init_cloud_texture();
    init_scene_materials();
    for(uint8_t id=0u;id<=SB_PICKUP_COUNT;++id) {
        g_gem_instances[id].mesh=&g_gem_mesh;
        g_gem_instances[id].materials=g_scene_materials;
        g_gem_instances[id].material_count=g_solid_pool.count;
        g_gem_instances[id].face_materials=g_gem_materials;
        g_gem_instances[id].world=&g_gem_world[id];
        g_gem_instances[id].pass=SB_PASS_ACTOR;
        g_gem_instances[id].cull_backfaces=0u;
    }
    loading_frame("IMPORTING PIG",10u);
    loading_frame("BUILDING SKY",13u);
    init_sky();
    loading_frame("BUILDING SEA",15u);
    init_sea();
    init_background();
    init_audio();
    sat_step_clock_init(&g_step_clock);
    for(;;) {
        uint32_t now;
        uint16_t steps;
        uint16_t pressed,events=0u;
        int32_t fx,fz,rx,rz;
#if SAT_SKYBRIDGE_VALIDATION
        uint16_t frame_start_frt;
#endif
        sat_example_must(sat_wait_vblank());
        g_metrics=(sb_frame_metrics_t){0};
        g_metrics.begin_ms=sat_time_ms();
#if SAT_SKYBRIDGE_VALIDATION
        frame_start_frt=sb_test_frt_counter();
#endif
        /* VDP2's register latch is at VBlank. Apply BOTH layer and sprite
         * priority configuration before the comparatively slow SMPC pad poll,
         * math, audio and VDP1 submissions. The color-calc PRISA selector
         * now lives in the layer shadow, so it cannot alternate per frame. */
        now=sat_frame_count();
        g_frame=now;
        sky_scroll.x_integer=sb_scenery_sky_scroll(
            (uint16_t)g_yaw,g_frame);
        sat_example_must(sat_vdp2_nbg0_set_scroll(&sky_scroll));
        animate_sea_palette(g_frame);
        update_rotation(sat_sin_deg(SB_F(g_yaw)),
                        sat_cos_deg(SB_F(g_yaw)));
        sat_example_must(sat_vdp2_layers_commit());
        {
            const uint32_t stage=sat_time_ms();
        sat_example_must(sat_pad_poll(&pad));
        /* Drop excess catch-up, preserve responsive input. sat_step_clock_steps
         * returns the raw elapsed count, so it can be 0 on a frame that beat
         * the display; this game always advances at least one tick rather than
         * freezing the simulation for that frame. */
#if SAT_SKYBRIDGE_VALIDATION
        /* Profile timing varies with the chosen execution policy under an
         * emulator. Keep test-route game state comparable while leaving the
         * production fixed-step catch-up policy unchanged. */
        steps=1u;
#else
        steps=sat_step_clock_steps(&g_step_clock,3u);
        if(steps==0u) steps=1u;
#endif
        /* Pause + X is a deliberate course selector for playing/testing
         * Course 2 without finishing all ten decks of Course 1 first. */
        if(g_game.paused && (pad.pressed&SAT_PAD_X)) {
            start_course((uint8_t)(g_game.course+1u));
            /* Keep the course picker open: with four courses, three
             * consecutive X presses select Course 4 without requiring
             * START/X/START/X/START/X to revisit the pause menu. */
            g_game.paused=1u;
        } else if (pad.pressed&SAT_PAD_START) {
            if(g_game.finished)
                start_course((uint8_t)(g_game.course+1u));
            else g_game.paused=(uint8_t)!g_game.paused;
        }
            g_metrics.input_ms=sat_time_ms()-stage;
        }
        if(pad.pressed&SAT_PAD_Y)g_show_debug=(uint8_t)!g_show_debug;
        if(pad.pressed&SAT_PAD_B) g_yaw-=15;
        if(pad.pressed&SAT_PAD_C) g_yaw+=15;
        if(g_yaw>=360)g_yaw-=360;
        if(g_yaw<0)g_yaw+=360;
        fx=sat_sin_deg(SB_F(g_yaw));
        fz=sat_cos_deg(SB_F(g_yaw));
        /* View basis: looking along +Z, screen-right is world -X. */
        sb_camera_right(fx,fz,&rx,&rz);
        pressed=0u;
        if (pad.pressed&SAT_PAD_A) pressed|=SB_JUMP;
        {
            uint16_t held=0u;
            const uint32_t physics_stage=sat_time_ms();
            if(pad.held&SAT_PAD_UP) held|=SB_UP;
            if(pad.held&SAT_PAD_DOWN) held|=SB_DOWN;
            if(pad.held&SAT_PAD_LEFT) held|=SB_LEFT;
            if(pad.held&SAT_PAD_RIGHT) held|=SB_RIGHT;
            if(pad.held&SAT_PAD_A) held|=SB_JUMP;
            if(pad.held&SAT_PAD_Z) held|=SB_BRAKE;
            while(steps--) {
                events|=sb_tick(&g_game,held,pressed,fx,fz,rx,rz);
                pressed=0u; /* A pressed edge is delivered once, never per catch-up step. */
            }
            g_metrics.physics_ms=sat_time_ms()-physics_stage;
        }
        /* Jump=2, Walk=0, Idle=1 in the selected converter clip order.
         * Use actual movement/grounding, not camera yaw, to choose poses. */
        {
            const uint32_t stage=sat_time_ms();
            uint16_t clip=g_game.support<0?2u:
                (sb_abs(g_game.vx)+sb_abs(g_game.vz)>SB_F(1)/3?0u:1u);
            if(clip!=g_pig_anim.clip)
                sat_example_must(sat_anim_set_clip(
                    &g_pig_anim,&skybridge_pig_anim_asset,clip));
            if(!g_game.paused && !g_game.finished)
                sat_example_must(sat_anim_advance(
                    &g_pig_anim,&skybridge_pig_anim_asset,SB_F(1)/60));
            g_metrics.animation_ms=sat_time_ms()-stage;
        }
        sound_event(events);
        {
            const uint32_t stage=sat_time_ms();
            int32_t ex,ez,lx,lz;
            const sat_vec3_t desired={g_game.x,g_game.y,g_game.z};
            sat_vec3_t eye_offset,target_offset;
            sb_camera_offset(fx,fz,&ex,&ez,&lx,&lz);
            eye_offset=(sat_vec3_t){ex,SB_F(29),ez};
            target_offset=(sat_vec3_t){lx,SB_F(5),lz};
            sat_example_must(sat_follow_camera3d_set_offsets(
                &g_follow_camera,&eye_offset,&target_offset));
            sat_example_must(sat_follow_camera3d_step(
                &g_follow_camera,&desired,(events&SB_EVENT_FALL)?1u:0u,
                &g_eye,&g_target));
            g_metrics.camera_ms=sat_time_ms()-stage;
        }
        g_camera.eye=g_eye;
        g_camera.target=g_target;
        sat_example_must(sat_camera3d_update(&g_camera));
        /* Sprite color calculation was configured at startup. The generic
         * VBlank layer replay now preserves both of its priority selectors. */
        sat_example_must(sat_vdp1_set_erase_transparent());
        sat_example_must(sat_begin_frame());
        g_world_cmd_full=0u;
        g_dbg_scene_status=SAT_OK;
        {
            const uint32_t submission_stage=sat_time_ms();
        draw_clouds();
        draw_world();
            g_metrics.submission_ms=sat_time_ms()-submission_stage;
        }
        record_parallel_snapshot();
        /* The HUD reports this completed CPU-side preparation interval. It is
         * intentionally sampled before HUD/VBlank work, so it describes the
         * frame whose diagnostics are on screen rather than a half-built
         * value from the next frame. */
        g_metrics.frame_cpu_ms=sat_time_ms()-g_metrics.begin_ms;
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.frame_cpu_frt_ticks=sb_test_frt_delta(frame_start_frt);
#endif
        sat_example_must(sat_vdp1_overlay_begin());
        {
            const uint32_t hud_stage=sat_time_ms();
        hud();
            g_metrics.hud_ms=sat_time_ms()-hud_stage;
        }
        sat_example_must(sat_end_frame());
        sat_example_must(sat_audio_update());
        g_metrics.frame_ms=sat_time_ms()-g_metrics.begin_ms;
#if SAT_SKYBRIDGE_VALIDATION
        g_metrics.frame_frt_ticks=sb_test_frt_delta(frame_start_frt);
#endif
        g_metrics.over_budget=(g_metrics.frame_ms>17u)?1u:0u;
#if SAT_SKYBRIDGE_VALIDATION
        write_test_telemetry(&pad);
#endif
    }
}
