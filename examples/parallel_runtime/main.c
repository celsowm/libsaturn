#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

#define OBJECT_COUNT 32u
#define VERTICES_PER_OBJECT 4u
#define VERTEX_COUNT (OBJECT_COUNT * VERTICES_PER_OBJECT)
#define FRAME_COUNT 2u
#define STARTUP_TIMEOUT 60000u

static sat_ascii_font_t g_font;
static int16_t g_positions[FRAME_COUNT * VERTEX_COUNT * 3u];
static sat_vec3_t g_output[VERTEX_COUNT];
static sat_vec3_t g_reference[VERTEX_COUNT];
static sat_anim_state_t g_state;
static sat_parallel_handle_t g_handle;
static sat_parallel_mode_t g_mode = SAT_PARALLEL_AUTO;
static sat_parallel_stats_t g_stats;
static uint32_t g_frame;
static uint32_t g_master_work;
static uint32_t g_errors;
static uint8_t g_validation = 1u;

static sat_model_asset_t g_model = {0};
static sat_model_animation_asset_t g_clip = {0};
static sat_animated_model_asset_t g_asset = {0};

static void make_animation_asset(void) {
    uint16_t frame;
    uint16_t object;
    g_model.vertex_count = VERTEX_COUNT;
    g_clip.positions = g_positions;
    g_clip.frame_count = FRAME_COUNT;
    g_clip.vertex_count = VERTEX_COUNT;
    g_clip.sample_rate_num = 30u;
    g_clip.sample_rate_den = 1u;
    g_clip.flags = SAT_ANIM_FLAG_LOOP;
    g_clip.encoding.bias_x = 0;
    g_clip.encoding.bias_y = 0;
    g_clip.encoding.bias_z = 0;
    g_clip.encoding.scale_x = SAT_FX16_ONE;
    g_clip.encoding.scale_y = SAT_FX16_ONE;
    g_clip.encoding.scale_z = SAT_FX16_ONE;
    for (frame = 0u; frame < FRAME_COUNT; ++frame) {
        for (object = 0u; object < OBJECT_COUNT; ++object) {
            const int16_t x = (int16_t)(-140 + (object % 8u) * 40u);
            const int16_t y = (int16_t)(-62 + (object / 8u) * 34u);
            const int16_t pulse = frame != 0u ? (int16_t)4 : (int16_t)-4;
            const uint32_t base =
                ((uint32_t)frame * VERTEX_COUNT + object * VERTICES_PER_OBJECT) * 3u;
            const int16_t corners[4][3] = {
                {(int16_t)(x - 8), (int16_t)(y - 8 + pulse), 0},
                {(int16_t)(x + 8), (int16_t)(y - 8 + pulse), 0},
                {(int16_t)(x + 8), (int16_t)(y + 8 + pulse), 0},
                {(int16_t)(x - 8), (int16_t)(y + 8 + pulse), 0}
            };
            for (uint16_t vertex = 0u; vertex < VERTICES_PER_OBJECT; ++vertex) {
                g_positions[base + vertex * 3u + 0u] = corners[vertex][0];
                g_positions[base + vertex * 3u + 1u] = corners[vertex][1];
                g_positions[base + vertex * 3u + 2u] = corners[vertex][2];
            }
        }
    }
    g_clip.encoding.scale_x = (sat_fx16_t)(SAT_FX16_ONE * 8);
    g_clip.encoding.scale_y = (sat_fx16_t)(SAT_FX16_ONE * 8);
    g_clip.encoding.scale_z = SAT_FX16_ONE;
    g_asset.model = &g_model;
    g_asset.animations = &g_clip;
    g_asset.animation_count = 1u;
}

static void draw_status(uint32_t wait_ticks) {
    const char* backend = g_mode == SAT_PARALLEL_MASTER ? "MASTER" :
        (g_mode == SAT_PARALLEL_SLAVE ? "SLAVE" : "AUTO");
    const char* selected = sat_parallel_backend() == SAT_PARALLEL_SLAVE ? "SLAVE" : "MASTER";
    sat_ascii_font_draw_text_screen_indexed8(&g_font, "LIBSATURN - PARALLEL RUNTIME", 8, 4, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, backend, 216, 4, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, "MASTER: RUNNING", 8, 18, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font,
        sat_parallel_slave_available() != 0u ? "SLAVE: RUNNING" : "SLAVE: OFFLINE",
        8, 30, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, "A: CHANGE BACKEND", 8, 208, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "OBJECTS ", OBJECT_COUNT, 8, 48, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "QUEUED  ", g_stats.queued, 8, 60, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "DONE    ", g_stats.completed, 8, 72, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "FAILED  ", g_stats.failed, 8, 84, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "MASTER W", g_master_work, 8, 96, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "WAIT    ", wait_ticks, 8, 108, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "TASK M  ", g_stats.master_tasks, 8, 120, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "TASK S  ", g_stats.slave_tasks, 8, 132, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, selected, 216, 18, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font,
        g_validation != 0u ? "VALIDATION: PASS" : "VALIDATION: FAIL",
        168, 208, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "ERRORS  ", g_errors, 216, 30, 8, 0u, 0u);
}

static void draw_objects(void) {
    for (uint16_t object = 0u; object < OBJECT_COUNT; ++object) {
        const sat_vec3_t* vertex = &g_output[object * VERTICES_PER_OBJECT];
        const int x = (vertex[0].x >> 16) + 160;
        const int y = (vertex[0].y >> 16) + 112;
        const uint16_t color = SAT_RGB555((uint8_t)(8u + object % 24u),
                                          (uint8_t)(20u + object % 10u), 31u);
        sat_draw_rect_screen((int16_t)x, (int16_t)y, 16u, 16u, color);
    }
}

static void cycle_backend(void) {
    if (sat_parallel_wait(g_handle, STARTUP_TIMEOUT) != SAT_OK) ++g_errors;
    (void)sat_parallel_release(g_handle);
    if (sat_parallel_shutdown(STARTUP_TIMEOUT) != SAT_OK) ++g_errors;
    g_mode = g_mode == SAT_PARALLEL_MASTER ? SAT_PARALLEL_SLAVE :
        (g_mode == SAT_PARALLEL_SLAVE ? SAT_PARALLEL_AUTO : SAT_PARALLEL_MASTER);
    sat_anim_parallel_register();
    sat_parallel_config_t config = {g_mode, 0, 0u, 0u, STARTUP_TIMEOUT};
    if (sat_parallel_init(&config) != SAT_OK) ++g_errors;
}

int main(void) {
    sat_pad_state_t pad = {0};
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 1u));
    make_animation_asset();
    sat_example_must(sat_anim_validate(&g_asset));
    sat_example_must(sat_anim_state_init(&g_state, &g_asset, 0u));
    sat_example_must(sat_anim_parallel_register());
    {
        sat_parallel_config_t config = {g_mode, 0, 0u, 0u, STARTUP_TIMEOUT};
        sat_example_must(sat_parallel_init(&config));
    }

    for (;;) {
        uint32_t wait_ticks = 0u;
        sat_example_must(sat_app_frame_begin(SAT_RGB555(1, 2, 8), SAT_RGB555(1, 2, 8), &pad));
        if ((pad.pressed & SAT_PAD_A) != 0u) cycle_backend();
        sat_anim_advance(&g_state, &g_asset, SAT_FX16_ONE / 60);
        sat_anim_decode(&g_asset, &g_state, g_reference, VERTEX_COUNT);
        {
            sat_anim_decode_job_t job = {&g_asset, &g_state, g_output, VERTEX_COUNT, 0u};
            const sat_result_t submitted = sat_anim_decode_async(&job, &g_handle);
            if (submitted != SAT_OK) ++g_errors;
        }
        /* Useful independent Master work: a checksum over the previous pose. */
        g_master_work = 0u;
        for (uint16_t i = 0u; i < VERTEX_COUNT; ++i) {
            g_master_work = g_master_work * 33u + (uint32_t)g_output[i].x;
        }
        if (sat_parallel_wait(g_handle, STARTUP_TIMEOUT) != SAT_OK) ++g_errors;
        for (uint16_t i = 0u; i < VERTEX_COUNT; ++i) {
            if (g_output[i].x != g_reference[i].x ||
                g_output[i].y != g_reference[i].y ||
                g_output[i].z != g_reference[i].z) {
                g_validation = 0u;
                ++g_errors;
                break;
            }
        }
        sat_parallel_stats(&g_stats);
        draw_objects();
        draw_status(wait_ticks);
        (void)sat_parallel_release(g_handle);
        ++g_frame;
        sat_example_must(sat_app_frame_end());
    }
}
