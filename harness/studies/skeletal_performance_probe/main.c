/* Isolated study wrapper: production skeletal demo remains unchanged.
 * Build this through a temporary examples/skeletal_performance_probe copy.
 */
#include "saturn/saturn.h"
#include "saturn/video.h"
#include "saturn/input.h"
#include "saturn/time.h"
#include "saturn/vdp1.h"
#include "saturn/irq.h"
#include "saturn/parallel.h"

extern uint32_t sat_scene3d_test_projection_ticks(void);
extern uint32_t sat_scene3d_test_prepare_ticks(void);
extern uint32_t sat_scene3d_test_painter_ticks(void);
extern uint32_t sat_scene3d_test_emit_ticks(void);
#if SKELETAL_STUDY_DUAL
static sat_result_t study_skin(const sat_skin_asset_t*, const sat_mat4_t*, uint16_t,
                              sat_mesh_t*, sat_fx16_t, sat_aabb3_t*);
#define sat_skin_update_mesh study_skin
#endif

static sat_result_t study_init(const sat_video_config_t* config);
static sat_result_t study_wait(void);
static sat_result_t study_pad(sat_pad_state_t* pad);
static sat_result_t study_end(void);

#define sat_init study_init
#define sat_wait_vblank study_wait
#define sat_pad_poll study_pad
#define sat_end_frame study_end
#include "examples/skeletal_animation_3d/robot_demo.inc"
#undef sat_init
#undef sat_wait_vblank
#undef sat_pad_poll
#undef sat_end_frame
#undef sat_skin_update_mesh

#define STUDY_ROWS 128u
typedef struct study_row {
    uint32_t phase, ccr, wait_ms, input_ms, sample_ms, skeleton_ms, skin_ms;
    uint32_t submit_ms, flush_ms, cpu_ms, end_ms, frame_ms;
    uint32_t commands, late_swaps, draw_waits, draw_timeouts;
    uint32_t vblank_end, display_frames_end;
    uint32_t projection_ticks, prepare_ticks, painter_ticks, emit_ticks;
} study_row_t;

volatile struct {
    uint32_t magic, initial_ccr, count, capacity;
    study_row_t rows[STUDY_ROWS];
} g_skeletal_study;
volatile struct {
    uint32_t init_result, slave_available, slave_tasks, master_tasks, errors, verified_poses;
} g_skeletal_study_parallel;
static uint32_t study_start, study_wait_ms, study_input_ms;
static uint32_t study_phase;

/* Sega dual-CPU bulletin section 6.2: purge CCR, then enable 4 KiB.
 * Run through the cache-through alias as required by the cache HAL.
 */
__attribute__((noinline)) static void study_enable_cache(void) {
    volatile uint8_t* ccr = (volatile uint8_t*)0xFFFFFE92u;
    *ccr = 0x10u;
    *ccr = 0x01u;
    __asm__ volatile ("" ::: "memory");
}

static sat_result_t study_init(const sat_video_config_t* config) {
    sat_result_t result = sat_init(config);
    g_skeletal_study.magic = 0x53545534u;
    g_skeletal_study.initial_ccr = *(volatile uint8_t*)0xFFFFFE92u;
    g_skeletal_study.capacity = STUDY_ROWS;
#if SKELETAL_STUDY_DUAL
    sat_parallel_config_t parallel = {SAT_PARALLEL_SLAVE, 0, 0u, 0u, 60000u};
    g_skeletal_study_parallel.init_result = (uint32_t)(-sat_parallel_init(&parallel));
    g_skeletal_study_parallel.slave_available = sat_parallel_slave_available();
#endif
    return result;
}

#if SKELETAL_STUDY_DUAL
typedef struct {
    const sat_skin_asset_t* skin;
    const sat_mat4_t* joints;
    uint16_t joint_count;
    sat_mesh_t* mesh;
    uint32_t split, count;
    sat_result_t status[2];
} study_skin_job_t;
static void study_skin_range(void* context, uint32_t first, uint32_t last) {
    study_skin_job_t* job = (study_skin_job_t*)context;
    for (uint32_t lane = first; lane < last; ++lane) {
        uint32_t begin = lane ? job->split : 0u;
        uint32_t end = lane ? job->count : job->split;
        job->status[lane] = sat_skin_deform_range(job->skin, job->joints,
            job->joint_count, begin, end - begin, job->mesh->vertices, job->mesh->vertex_cap);
    }
}
static sat_result_t study_skin(const sat_skin_asset_t* skin, const sat_mat4_t* joints,
                              uint16_t joint_count, sat_mesh_t* mesh,
                              sat_fx16_t margin, sat_aabb3_t* bounds) {
    study_skin_job_t job = {skin, joints, joint_count, mesh,
        skin->rigid_count + skin->blend_count / 2u, sat_skin_vertex_count(skin), {SAT_OK, SAT_OK}};
    sat_result_t result = sat_parallel_for(0u, 2u, 1u, study_skin_range, &job,
        mesh->vertices, job.count * sizeof(sat_vec3_t));
    // Status slots also cross CPU ownership, unlike the immutable descriptor.
    sat_parallel_cache_sync_range(job.status, sizeof(job.status));
    if (result != SAT_OK || job.status[0] != SAT_OK || job.status[1] != SAT_OK) {
        ++g_skeletal_study_parallel.errors;
        return result != SAT_OK ? result : (job.status[0] != SAT_OK ? job.status[0] : job.status[1]);
    }
    mesh->vertex_count = (uint16_t)job.count;
    // Verify the first three actual poses against the canonical serial path.
    // These warm-up samples are excluded by the analysis script.
    if (g_skeletal_study_parallel.verified_poses < 3u) {
        static sat_vec3_t reference[ROBOT_POLICE_SKELETAL_VERTEX_COUNT];
        sat_result_t check = sat_skin_deform(skin, joints, joint_count,
            reference, ROBOT_POLICE_SKELETAL_VERTEX_COUNT);
        if (check != SAT_OK) return check;
        for (uint32_t v = 0; v < job.count; ++v) {
            if (reference[v].x != mesh->vertices[v].x ||
                reference[v].y != mesh->vertices[v].y ||
                reference[v].z != mesh->vertices[v].z) {
                ++g_skeletal_study_parallel.errors;
                return SAT_ERR_VERIFY_FAILED;
            }
        }
        ++g_skeletal_study_parallel.verified_poses;
    }
    if (bounds) return sat_skin_bounds(mesh->vertices, job.count, margin, bounds);
    return SAT_OK;
}
#endif

static sat_result_t study_wait(void) {
    /* Loading-screen calls do not count as animation samples. */
    if (g_skeletal_study.count == 20u && !study_phase) {
        void (*enable)(void) = (void (*)(void))((uintptr_t)&study_enable_cache | 0x20000000u);
        enable();
        study_phase = 1u;
    }
    study_start = sat_time_ms();
    sat_result_t result = sat_wait_vblank();
    study_wait_ms = sat_time_ms() - study_start;
    return result;
}

static sat_result_t study_pad(sat_pad_state_t* pad) {
    uint32_t start = sat_time_ms();
    sat_result_t result = sat_pad_poll(pad);
    study_input_ms = sat_time_ms() - start;
    return result;
}

static sat_result_t study_end(void) {
    uint32_t start = sat_time_ms();
    sat_result_t result = sat_end_frame();
    uint32_t end_ms = sat_time_ms() - start;
    uint32_t frame_ms = sat_time_ms() - study_start;
    if (g_skeletal_profile.frames && g_skeletal_study.count < STUDY_ROWS) {
        sat_vdp1_command_stats_t commands = {0};
        sat_vdp1_frame_stats_t frame = {0};
        sat_vdp1_command_stats(&commands);
        sat_vdp1_frame_stats(&frame);
        uint32_t index = g_skeletal_study.count;
        volatile study_row_t* row = &g_skeletal_study.rows[index];
        row->phase = study_phase;
        row->ccr = *(volatile uint8_t*)0xFFFFFE92u;
        row->wait_ms = study_wait_ms;
        row->input_ms = study_input_ms;
        row->sample_ms = g_skeletal_profile.sample_ms;
        row->skeleton_ms = g_skeletal_profile.skeleton_ms;
        row->skin_ms = g_skeletal_profile.skin_ms;
        row->submit_ms = g_skeletal_profile.submit_ms;
        row->flush_ms = g_skeletal_profile.flush_ms;
        row->cpu_ms = g_skeletal_profile.total_ms;
        row->end_ms = end_ms;
        row->frame_ms = frame_ms;
        row->commands = commands.used;
        row->late_swaps = frame.late_swaps;
        row->draw_waits = frame.draw_waits;
        row->draw_timeouts = frame.draw_timeouts;
        row->vblank_end = sat_irq_count(SAT_IRQ_VBLANK_IN);
        row->display_frames_end = sat_frame_count();
        row->projection_ticks = sat_scene3d_test_projection_ticks();
        row->prepare_ticks = sat_scene3d_test_prepare_ticks();
        row->painter_ticks = sat_scene3d_test_painter_ticks();
        row->emit_ticks = sat_scene3d_test_emit_ticks();
        sat_parallel_stats_t parallel = {0};
        if (sat_parallel_stats(&parallel) == SAT_OK) {
            g_skeletal_study_parallel.slave_tasks = parallel.slave_tasks;
            g_skeletal_study_parallel.master_tasks = parallel.master_tasks;
        }
        g_skeletal_study.count = index + 1u;
    }
    return result;
}
