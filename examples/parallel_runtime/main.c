#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

#define OBJECT_COUNT 32u
#define VERTICES_PER_OBJECT 4u
#define VERTEX_COUNT (OBJECT_COUNT * VERTICES_PER_OBJECT)
#define FRAME_COUNT 2u
#define STARTUP_TIMEOUT 60000u
#define SCREEN_W 320u
#define SCREEN_H 224u
#ifndef SAT_PARALLEL_RUNTIME_GEOMETRY_OBJECTS
#define SAT_PARALLEL_RUNTIME_GEOMETRY_OBJECTS 12u
#endif
#ifndef SAT_PARALLEL_RUNTIME_DEFAULT_MODE
#define SAT_PARALLEL_RUNTIME_DEFAULT_MODE 2
#endif
#define GEOMETRY_OBJECT_COUNT SAT_PARALLEL_RUNTIME_GEOMETRY_OBJECTS
#define GEOMETRY_FACE_CAP GEOMETRY_OBJECT_COUNT
#define ANIMATION_BUFFER_COUNT 2u

static sat_ascii_font_t g_font;
static int16_t g_positions[FRAME_COUNT * VERTEX_COUNT * 3u];
static sat_vec3_t g_output[ANIMATION_BUFFER_COUNT][VERTEX_COUNT];
static sat_vec3_t g_reference[VERTEX_COUNT];
static sat_anim_state_t g_state;
static sat_parallel_handle_t g_handle;
static sat_parallel_mode_t g_mode =
    (sat_parallel_mode_t)SAT_PARALLEL_RUNTIME_DEFAULT_MODE;
static sat_parallel_stats_t g_stats;
static uint32_t g_frame;
static uint32_t g_master_work;
static uint32_t g_errors;
static uint8_t g_validation = 1u;
static uint8_t g_geometry_validation = 1u;
static uint8_t g_geometry_handle_valid;
static uint8_t g_animation_handle_valid;
static uint8_t g_animation_read_buffer;
static uint32_t g_frame_ticks;
static uint32_t g_geometry_master_work;
static uint32_t g_previous_wait_ticks;
static uint32_t g_previous_submission_ticks;

static sat_model_asset_t g_model = {0};
static sat_model_animation_asset_t g_clip = {0};
static sat_animated_model_asset_t g_asset = {0};

static sat_scene_t g_scene;
static sat_scene3d_face_t g_scene_faces[GEOMETRY_FACE_CAP];
static uint32_t g_scene_keys[GEOMETRY_FACE_CAP];
static uint16_t g_scene_order[GEOMETRY_FACE_CAP];
static sat_camera3d_t g_camera;
static sat_mesh_t g_geometry_mesh;
static sat_vec3_t g_geometry_vertices[4];
static uint16_t g_geometry_indices[4] = {0u, 1u, 2u, 3u};
static sat_scene3d_material_t g_geometry_materials[1];
static uint16_t g_geometry_face_materials[1] = {0u};
static sat_scene3d_instance_t g_geometry_instances[GEOMETRY_OBJECT_COUNT];
static sat_mat4_t g_geometry_world[GEOMETRY_OBJECT_COUNT];
static sat_projected_vertex_t g_geometry_screen[GEOMETRY_OBJECT_COUNT][4];
static sat_vec3_t g_geometry_scratch[GEOMETRY_OBJECT_COUNT][4];
static sat_scene3d_prepare_item_t g_geometry_items[GEOMETRY_OBJECT_COUNT];
static sat_scene3d_face_t g_geometry_faces[GEOMETRY_FACE_CAP];
static uint32_t g_geometry_keys[GEOMETRY_FACE_CAP];
static uint16_t g_geometry_order[GEOMETRY_FACE_CAP];
static sat_scene3d_face_t g_geometry_reference_faces[GEOMETRY_FACE_CAP];
static uint32_t g_geometry_reference_keys[GEOMETRY_FACE_CAP];
static uint16_t g_geometry_reference_order[GEOMETRY_FACE_CAP];
static sat_scene3d_prepare_batch_t g_geometry_batch;
static sat_scene3d_prepare_batch_t g_geometry_reference_batch;
static sat_parallel_handle_t g_geometry_handle;

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

static void make_geometry_scene(void) {
    uint16_t vertex_index = 0u;
    sat_example_must(sat_mesh_init(
        &g_geometry_mesh, g_geometry_vertices, 4u,
        g_geometry_indices, 1u));
    sat_example_must(sat_mesh_add_vertex(
        &g_geometry_mesh, -SAT_FX16_ONE, -SAT_FX16_ONE, 0, &vertex_index));
    sat_example_must(sat_mesh_add_vertex(
        &g_geometry_mesh, SAT_FX16_ONE, -SAT_FX16_ONE, 0, &vertex_index));
    sat_example_must(sat_mesh_add_vertex(
        &g_geometry_mesh, SAT_FX16_ONE, SAT_FX16_ONE, 0, &vertex_index));
    sat_example_must(sat_mesh_add_vertex(
        &g_geometry_mesh, -SAT_FX16_ONE, SAT_FX16_ONE, 0, &vertex_index));
    sat_example_must(sat_mesh_add_face(&g_geometry_mesh, 0u, 1u, 2u, 3u));
    g_geometry_materials[0] = (sat_scene3d_material_t){
        SAT_SCENE3D_RGB, SAT_RGB555(20u, 10u, 28u), 0, 0,
        SAT_INDEXED_SOLID_OPAQUE, 0};
    for (uint16_t i = 0u; i < GEOMETRY_OBJECT_COUNT; ++i) {
        g_geometry_instances[i] = (sat_scene3d_instance_t){
            &g_geometry_mesh, g_geometry_materials, 1u,
            g_geometry_face_materials, &g_geometry_world[i], 0u, 0u};
        g_geometry_items[i] = (sat_scene3d_prepare_item_t){
            &g_geometry_instances[i], g_geometry_screen[i],
            g_geometry_scratch[i], SAT_SCENE3D_SLOT_INHERIT, 0u};
    }
    sat_example_must(sat_scene3d_prepare_batch_init(
        &g_geometry_batch, g_geometry_items, GEOMETRY_OBJECT_COUNT,
        g_geometry_faces, g_geometry_keys, g_geometry_order,
        GEOMETRY_FACE_CAP));
    sat_example_must(sat_scene3d_prepare_batch_init(
        &g_geometry_reference_batch, g_geometry_items, GEOMETRY_OBJECT_COUNT,
        g_geometry_reference_faces, g_geometry_reference_keys,
        g_geometry_reference_order, GEOMETRY_FACE_CAP));
    g_geometry_materials[0].rgb555 = SAT_RGB555(20u, 10u, 28u);
}

static void update_geometry_world(uint32_t frame) {
    for (uint16_t i = 0u; i < GEOMETRY_OBJECT_COUNT; ++i) {
        const int16_t x = (int16_t)(-9 + (i % 4u) * 6);
        const int16_t y = (int16_t)(-5 + (i / 4u) * 5);
        const int16_t z = (i == 0u && (frame % 60u) >= 30u)
            ? (int16_t)1 : (int16_t)(8 + (i % 3u) * 3);
        const sat_fx16_t wobble = (sat_fx16_t)(((frame + i * 7u) % 20u) - 10) *
            (SAT_FX16_ONE / 32);
        sat_example_must(sat_mat4_translate(
            &g_geometry_world[i], sat_fx16_from_int(x) + wobble,
            sat_fx16_from_int(y), -sat_fx16_from_int(z)));
    }
}

static uint8_t equal_texture(const sat_vdp1_texture_t* a,
                             const sat_vdp1_texture_t* b) {
    if (a == 0 || b == 0) return a == b ? 1u : 0u;
    return a->srca == b->srca && a->width == b->width &&
        a->height == b->height && a->palette == b->palette &&
        a->valid == b->valid ? 1u : 0u;
}

static uint8_t equal_tiled(const sat_indexed_tiled_quad3_t* a,
                           const sat_indexed_tiled_quad3_t* b) {
    if (a == 0 || b == 0) return a == b ? 1u : 0u;
    if (equal_texture(a->full, b->full) == 0u) return 0u;
    for (uint8_t i = 0u; i < 4u; ++i)
        if (equal_texture(a->tiles[i], b->tiles[i]) == 0u) return 0u;
    return 1u;
}

static uint8_t equal_material(const sat_scene3d_material_t* a,
                              const sat_scene3d_material_t* b) {
    return a->kind == b->kind && a->rgb555 == b->rgb555 &&
        a->color_calc_slot == b->color_calc_slot &&
        a->vertex_gouraud == 0 && b->vertex_gouraud == 0 &&
        equal_texture(a->texture, b->texture) != 0u &&
        equal_tiled(a->tiled, b->tiled) != 0u ? 1u : 0u;
}

static uint8_t equal_face(const sat_scene3d_face_t* a,
                          const sat_scene3d_face_t* b) {
    if (a->projected_safe != b->projected_safe ||
        a->gouraud_valid != b->gouraud_valid ||
        equal_material(&a->material, &b->material) == 0u) return 0u;
    for (uint8_t i = 0u; i < 4u; ++i) {
        if (a->world.v[i].x != b->world.v[i].x ||
            a->world.v[i].y != b->world.v[i].y ||
            a->world.v[i].z != b->world.v[i].z ||
            a->projected.x[i] != b->projected.x[i] ||
            a->projected.y[i] != b->projected.y[i] ||
            (a->gouraud_valid != 0u && a->gouraud[i] != b->gouraud[i])) {
            return 0u;
        }
    }
    return 1u;
}

static void validate_geometry_output(void) {
    g_geometry_reference_batch.view_proj = g_geometry_batch.view_proj;
    g_geometry_reference_batch.eye = g_geometry_batch.eye;
    g_geometry_reference_batch.forward = g_geometry_batch.forward;
    g_geometry_reference_batch.near_depth = g_geometry_batch.near_depth;
    g_geometry_reference_batch.width = g_geometry_batch.width;
    g_geometry_reference_batch.height = g_geometry_batch.height;
    if (sat_scene3d_prepare_batch_execute(&g_geometry_reference_batch) != SAT_OK ||
        g_geometry_reference_batch.metrics.source_faces !=
            g_geometry_batch.metrics.source_faces ||
        g_geometry_reference_batch.metrics.prepared_faces !=
            g_geometry_batch.metrics.prepared_faces ||
        g_geometry_reference_batch.metrics.culled_faces !=
            g_geometry_batch.metrics.culled_faces ||
        g_geometry_reference_batch.metrics.clipped_faces !=
            g_geometry_batch.metrics.clipped_faces) {
        g_geometry_validation = 0u;
        return;
    }
    for (uint16_t i = 0u; i < g_geometry_batch.metrics.prepared_faces; ++i) {
        if (g_geometry_keys[i] != g_geometry_reference_keys[i] ||
            g_geometry_order[i] != g_geometry_reference_order[i] ||
            equal_face(&g_geometry_faces[i], &g_geometry_reference_faces[i]) == 0u) {
            g_geometry_validation = 0u;
            return;
        }
    }
}

static void draw_status(uint32_t wait_ticks, uint32_t submission_ticks) {
    const char* backend = g_mode == SAT_PARALLEL_MASTER ? "MASTER" :
        (g_mode == SAT_PARALLEL_SLAVE ? "SLAVE" : "AUTO");
    const char* selected = sat_parallel_backend() == SAT_PARALLEL_SLAVE ? "SLAVE" : "MASTER";
    sat_ascii_font_draw_text_screen_indexed8(&g_font, "LIBSATURN - PARALLEL RUNTIME", 8, 4, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, backend, 216, 4, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, "MASTER: RUNNING", 8, 18, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font,
        sat_parallel_slave_available() != 0u ? "SLAVE: READY" :
        (g_errors != 0u ? "SLAVE: ERROR" : "SLAVE: OFFLINE"),
        8, 30, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, "A: CHANGE BACKEND", 8, 208, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "OBJECTS ", OBJECT_COUNT, 8, 48, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "GEO OBJ ", GEOMETRY_OBJECT_COUNT, 8, 60, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "GEO FACE", g_geometry_batch.metrics.prepared_faces, 8, 72, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "QUEUED  ", g_stats.queued, 8, 84, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "DONE    ", g_stats.completed, 8, 96, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "FAILED  ", g_stats.failed, 8, 108, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "MASTER W", g_master_work, 8, 120, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "GEO WORK", g_geometry_master_work, 8, 132, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "SUBMIT  ", submission_ticks, 168, 48, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "WAIT    ", wait_ticks, 168, 60, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "FRAME   ", g_frame_ticks, 168, 72, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "TASK M  ", g_stats.master_tasks, 168, 84, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "TASK S  ", g_stats.slave_tasks, 168, 96, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "LAST    ", g_stats.last_task_ticks, 168, 108, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font, selected, 216, 18, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font,
        g_validation != 0u ? "ANIM VALIDATION: PASS" : "ANIM VALIDATION: FAIL",
        8, 184, 8, 0u, 0u);
    sat_ascii_font_draw_text_screen_indexed8(&g_font,
        g_geometry_validation != 0u
            ? "GEOMETRY VALIDATION: PASS" : "GEOMETRY VALIDATION: FAIL",
        8, 196, 8, 0u, 0u);
    sat_ascii_font_draw_label_u32(&g_font, "ERRORS  ", g_errors, 216, 30, 8, 0u, 0u);
}

static void draw_objects(void) {
    const sat_vec3_t* const output = g_output[g_animation_read_buffer];
    for (uint16_t object = 0u; object < OBJECT_COUNT; ++object) {
        const sat_vec3_t* vertex = &output[object * VERTICES_PER_OBJECT];
        const int x = (vertex[0].x >> 16) + 160;
        const int y = (vertex[0].y >> 16) + 112;
        const uint16_t color = SAT_RGB555((uint8_t)(8u + object % 24u),
                                          (uint8_t)(20u + object % 10u), 31u);
        sat_draw_rect_screen((int16_t)x, (int16_t)y, 16u, 16u, color);
    }
}

static void cycle_backend(void) {
    uint8_t safe_to_switch = 1u;
    if (g_animation_handle_valid != 0u) {
        const sat_result_t waited = sat_parallel_wait(g_handle, STARTUP_TIMEOUT);
        if (waited != SAT_OK &&
            sat_parallel_state(g_handle) == SAT_PARALLEL_RUNNING) {
            ++g_errors;
            if (sat_parallel_abort(g_handle, STARTUP_TIMEOUT) != SAT_OK) ++g_errors;
        }
        if (sat_parallel_state(g_handle) != SAT_PARALLEL_RUNNING) {
            if (sat_parallel_release(g_handle) != SAT_OK) ++g_errors;
            g_animation_handle_valid = 0u;
        } else safe_to_switch = 0u;
    }
    if (g_geometry_handle_valid != 0u) {
        const sat_result_t waited = sat_parallel_wait(
            g_geometry_handle, STARTUP_TIMEOUT);
        if (waited != SAT_OK && sat_parallel_state(g_geometry_handle) ==
            SAT_PARALLEL_RUNNING) {
            ++g_errors;
            if (sat_parallel_abort(g_geometry_handle, STARTUP_TIMEOUT) != SAT_OK) ++g_errors;
        }
        if (sat_parallel_state(g_geometry_handle) != SAT_PARALLEL_RUNNING) {
            if (sat_scene_prepare_batch_release(
                    &g_geometry_batch, g_geometry_handle) != SAT_OK) ++g_errors;
            g_geometry_handle_valid = 0u;
        } else safe_to_switch = 0u;
    }
    if (sat_parallel_shutdown(STARTUP_TIMEOUT) != SAT_OK) {
        ++g_errors;
        safe_to_switch = 0u;
    }
    if (safe_to_switch == 0u) return;
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
    make_geometry_scene();
    sat_example_must(sat_anim_validate(&g_asset));
    sat_example_must(sat_anim_state_init(&g_state, &g_asset, 0u));
    sat_example_must(sat_anim_parallel_register());
    {
        sat_parallel_config_t config = {g_mode, 0, 0u, 0u, STARTUP_TIMEOUT};
        sat_example_must(sat_parallel_init(&config));
    }
    sat_example_must(sat_scene_init(
        &g_scene, g_scene_faces, g_scene_keys, g_scene_order,
        GEOMETRY_FACE_CAP));
    sat_example_must(sat_camera3d_init(
        &g_camera,
        &(sat_vec3_t){0, 0, 0}, &(sat_vec3_t){0, 0, -SAT_FX16_ONE},
        &(sat_vec3_t){0, SAT_FX16_ONE, 0}, sat_fx16_from_int(60),
        sat_fx16_div(sat_fx16_from_int(SCREEN_W), sat_fx16_from_int(SCREEN_H)),
        sat_fx16_from_int(1), sat_fx16_from_int(100)));
    sat_example_must(sat_anim_decode(
        &g_asset, &g_state, g_output[0], VERTEX_COUNT));
    g_animation_read_buffer = 0u;

    for (;;) {
        uint32_t wait_ticks = 0u;
        uint8_t animation_write_buffer =
            (uint8_t)(1u - g_animation_read_buffer);
        uint8_t animation_ready = 0u;
        const uint32_t frame_start = sat_time_ms();
        sat_example_must(sat_app_frame_begin(SAT_RGB555(1, 2, 8), SAT_RGB555(1, 2, 8), &pad));
        if ((pad.pressed & SAT_PAD_A) != 0u) cycle_backend();
        sat_anim_advance(&g_state, &g_asset, SAT_FX16_ONE / 60);
        sat_anim_decode(&g_asset, &g_state, g_reference, VERTEX_COUNT);
        g_camera.target.x = ((g_frame / 30u) & 1u) != 0u
            ? sat_fx16_from_int(1) : -sat_fx16_from_int(1);
        sat_example_must(sat_camera3d_update(&g_camera));
        update_geometry_world(g_frame);
        sat_example_must(sat_scene_begin(
            &g_scene, &g_camera, sat_fx16_from_int(1), SCREEN_W, SCREEN_H, 0u));
        {
            const sat_result_t submitted = sat_scene_prepare_batch_async(
                &g_scene, &g_geometry_batch, &g_geometry_handle);
            if (submitted != SAT_OK) ++g_errors;
            else g_geometry_handle_valid = 1u;
        }
        {
            sat_anim_decode_job_t job = {
                &g_asset, &g_state, g_output[animation_write_buffer],
                VERTEX_COUNT, 0u};
            const sat_result_t submitted = sat_anim_decode_async(&job, &g_handle);
            if (submitted != SAT_OK) {
                ++g_errors;
                sat_example_must(sat_anim_decode(
                    &g_asset, &g_state, g_output[animation_write_buffer],
                    VERTEX_COUNT));
                animation_ready = 1u;
            } else {
                g_animation_handle_valid = 1u;
            }
        }
        /* The Slave exclusively owns the write buffer. The Master only reads
         * the completed previous buffer while the current decode runs. */
        g_master_work = 0u;
        for (uint16_t i = 0u; i < VERTEX_COUNT; ++i) {
            g_master_work = g_master_work * 33u +
                (uint32_t)g_output[g_animation_read_buffer][i].x;
        }
        /* Useful independent Master work while the geometry task is active. */
        g_geometry_master_work = 0u;
        for (uint16_t i = 0u; i < GEOMETRY_OBJECT_COUNT; ++i) {
            g_geometry_master_work = g_geometry_master_work * 33u +
                (uint32_t)g_geometry_world[i].m[3];
        }
        if (g_geometry_handle_valid != 0u) {
            const sat_result_t waited = sat_parallel_wait(
                g_geometry_handle, STARTUP_TIMEOUT);
            if (waited != SAT_OK && sat_parallel_state(g_geometry_handle) ==
                SAT_PARALLEL_RUNNING) {
                ++g_errors;
                if (sat_parallel_abort(
                        g_geometry_handle, STARTUP_TIMEOUT) != SAT_OK) {
                    ++g_errors;
                    sat_example_must(SAT_ERR_BUSY);
                }
            }
            if (sat_parallel_state(g_geometry_handle) != SAT_PARALLEL_RUNNING) {
                if (waited == SAT_OK) {
                if (g_frame < 3u || (g_frame % 30u) == 0u)
                    validate_geometry_output();
                if (sat_scene_merge_prepared_batch(
                        &g_scene, &g_geometry_batch, g_geometry_handle) != SAT_OK)
                    ++g_errors;
                }
                if (sat_scene_prepare_batch_release(
                        &g_geometry_batch, g_geometry_handle) != SAT_OK) ++g_errors;
                g_geometry_handle_valid = 0u;
            }
        }
        if (g_animation_handle_valid != 0u) {
            const sat_result_t waited = sat_parallel_wait(g_handle, STARTUP_TIMEOUT);
            if (waited != SAT_OK && sat_parallel_state(g_handle) ==
                SAT_PARALLEL_RUNNING) {
                ++g_errors;
                if (sat_parallel_abort(g_handle, STARTUP_TIMEOUT) != SAT_OK) {
                    ++g_errors;
                    sat_example_must(SAT_ERR_BUSY);
                }
            }
            if (sat_parallel_state(g_handle) != SAT_PARALLEL_RUNNING) {
                if (waited == SAT_OK) animation_ready = 1u;
                if (animation_ready != 0u) {
                    for (uint16_t i = 0u; i < VERTEX_COUNT; ++i) {
                        if (g_output[animation_write_buffer][i].x != g_reference[i].x ||
                            g_output[animation_write_buffer][i].y != g_reference[i].y ||
                            g_output[animation_write_buffer][i].z != g_reference[i].z) {
                            g_validation = 0u;
                            ++g_errors;
                            break;
                        }
                    }
                    g_animation_read_buffer = animation_write_buffer;
                }
                if (sat_parallel_release(g_handle) != SAT_OK) ++g_errors;
                g_animation_handle_valid = 0u;
            }
        } else if (animation_ready != 0u) {
            for (uint16_t i = 0u; i < VERTEX_COUNT; ++i) {
                if (g_output[animation_write_buffer][i].x != g_reference[i].x ||
                    g_output[animation_write_buffer][i].y != g_reference[i].y ||
                    g_output[animation_write_buffer][i].z != g_reference[i].z) {
                    g_validation = 0u;
                    ++g_errors;
                    break;
                }
            }
            g_animation_read_buffer = animation_write_buffer;
        } else {
            g_validation = 0u;
        }
        if (sat_scene_flush(&g_scene) != SAT_OK) ++g_errors;
        sat_parallel_stats(&g_stats);
        g_frame_ticks = sat_time_ms() - frame_start;
        wait_ticks = g_stats.master_wait_ticks - g_previous_wait_ticks;
        const uint32_t submission_ticks =
            g_stats.submission_ticks - g_previous_submission_ticks;
        g_previous_wait_ticks = g_stats.master_wait_ticks;
        g_previous_submission_ticks = g_stats.submission_ticks;
        draw_objects();
        draw_status(wait_ticks, submission_ticks);
        ++g_frame;
        sat_example_must(sat_app_frame_end());
    }
}
