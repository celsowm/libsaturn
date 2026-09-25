/* Skybridge 3D: validation-build telemetry block read by the harness. */
#include "skybridge.h"

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

uint16_t sb_test_frt_counter(void) {
    volatile uint8_t* const high=(volatile uint8_t*)0xFFFFFE12u;
    volatile uint8_t* const low=(volatile uint8_t*)0xFFFFFE13u;
    const uint16_t h=*high;
    return (uint16_t)((h<<8u)|*low);
}

uint32_t sb_test_frt_delta(uint16_t start) {
    return (uint16_t)(sb_test_frt_counter()-start);
}
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

void initialize_test_telemetry(void) {
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

void write_test_telemetry(const sat_pad_state_t* pad) {
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
