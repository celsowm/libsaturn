#include "lab.h"
#include "example_util.h"
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/time.h"

enum { FACE_CAPACITY = 1024,
       OVERLAY_COMMANDS = 320 };

static sat_scene_t scene;
static sat_scene3d_face_t faces[FACE_CAPACITY];
static uint32_t keys[FACE_CAPACITY];
static uint16_t order[FACE_CAPACITY];
static sat_fx_compositor_t compositor;

int main(void) {
    static const uint16_t workloads[] = {128, 256, LAB_MAX_WORKLOAD};
    const sat_video_config_t video = {LAB_WIDTH, LAB_HEIGHT, SAT_VIDEO_AUTO, 0};
    sat_camera3d_t camera;
    lab_player_t player;
    uint32_t tick = 0;
    unsigned stress = 0;
    int32_t orbit = 0;
    uint8_t automatic = 1;
    uint8_t diagnostics = 0;
    uint8_t paused = 0;
    uint8_t camera_immersed = 0;
    sat_water_volume_t volume = {
        .min_x = LAB_UNIT(-8), .max_x = LAB_UNIT(8),
        .min_z = LAB_UNIT(-8), .max_z = LAB_UNIT(8),
        .bottom = LAB_UNIT(-3), .surface = LAB_UNIT(-1),
        .current_x = SAT_FX16_ONE / 80,
        .buoyancy = SAT_FX16_ONE / 26, .drag = SAT_FX16_ONE / 5
    };
    const lab_water_t default_water = {LAB_POOL, LAB_UNIT(-1), 8, 1};
    lab_water_t water = default_water;
    uint8_t fx_reserved = 0;

    sat_example_must(sat_init(&video));
    sat_example_must(sat_vdp1_set_frame_mode(SAT_VDP1_FRAME_ON_SUBMIT));
    sat_example_must(sat_vdp2_back_color_set(SAT_RGB555(3, 5, 8)));
    sat_example_must(sat_set_clear_color(SAT_RGB555(3, 5, 8)));
    sat_example_must(sat_scene_init(&scene, faces, keys, order, FACE_CAPACITY));
    lab_surface_init();
    lab_scene_init();
    lab_benchmark_init();
    lab_player_reset(&player);

    for (;;) {
        sat_pad_state_t pad = {0};
        /* Build in the free command bank while the previous draw or swap
         * finishes. ON_SUBMIT waits before publishing this bank. */
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_pad_poll(&pad));
        if (pad.pressed & SAT_PAD_START) {
            lab_player_reset(&player);
            tick = 0;
            stress = 0;
            orbit = 0;
            automatic = 1;
            diagnostics = 0;
            paused = 0;
            water = default_water;
            camera_immersed = 0;
        } else {
            if (pad.pressed & SAT_PAD_C) automatic = (automatic + 1u) % 3u;
            const uint16_t chords = SAT_PAD_X | SAT_PAD_Y | SAT_PAD_A | SAT_PAD_B;
            if ((pad.pressed & SAT_PAD_Z) && !(pad.held & chords)) diagnostics ^= 1u;
            if (pad.held & SAT_PAD_Z) {
                if (pad.pressed & SAT_PAD_X) paused ^= 1u;
                if (pad.pressed & SAT_PAD_Y) stress = (stress + 1u) % 3u;
                if (pad.pressed & SAT_PAD_B) {
                    water.subdivisions = water.subdivisions == 16 ? 4 : water.subdivisions * 2;
                }
                if (pad.pressed & SAT_PAD_A) water.waves ^= 1u;
                pad.pressed &= (uint16_t)~SAT_PAD_A;
                pad.held &= (uint16_t)~(SAT_PAD_A | SAT_PAD_B);
            } else {
                if (pad.pressed & SAT_PAD_X) water.mode = (water.mode + 1u) % LAB_MODE_COUNT;
                if (pad.pressed & SAT_PAD_Y) {
                    water.height -= SAT_FX16_ONE / 2;
                    if (water.height < LAB_UNIT(-2)) water.height = -SAT_FX16_ONE / 2;
                }
            }
            if (pad.held & SAT_PAD_L) orbit = (orbit + 358) % 360;
            if (pad.held & SAT_PAD_R) orbit = (orbit + 2) % 360;
            volume.surface = water.height;
            if (!paused) lab_player_step(&player, &pad, &volume);
        }

        volume.surface = water.height;
        lab_camera_update(&camera, &player, tick, orbit, automatic);
        sat_example_must(sat_water_volume_immersion(
            &volume, &camera.eye, SAT_FX16_ONE / 16, camera_immersed, &camera_immersed));
        const uint64_t prepare = sat_time_ticks();
        sat_example_must(sat_scene_begin(&scene, &camera, LAB_NEAR, LAB_WIDTH,
                                        LAB_HEIGHT, OVERLAY_COMMANDS));
        lab_scene_draw(&scene, &player, tick, workloads[stress], &water);
        const uint64_t surface_begin = sat_time_ticks();
        lab_surface_draw(&scene, &water, tick);
        const uint64_t flush = sat_time_ticks();
        sat_vdp1_command_checkpoint_t first, last;
        sat_example_must(sat_vdp1_command_checkpoint(&first));
        sat_example_must(sat_scene_flush(&scene));
        lab_benchmark_scene_timing((uint32_t)(flush - prepare), (uint32_t)(flush - surface_begin),
                                   (uint32_t)(sat_time_ticks() - flush));
        sat_result_t fx_result = SAT_OK;
        if (water.mode == LAB_MULTIPASS) {
            if (!fx_reserved) {
                fx_result = sat_fx_compositor_reserve(&compositor);
                fx_reserved = fx_result == SAT_OK;
            }
            if (fx_reserved) {
                const sat_fx_mask_t masks[] = {
                    {4, 6, 310, diagnostics ? 84 : 23}, {4, paused ? 193 : 205, 310, 17}
                };
                sat_example_must(sat_vdp1_command_checkpoint(&last));
                fx_result = sat_fx_compositor_prepare(&compositor, &first, &last, masks, 2);
            }
        }
        lab_benchmark_draw(&scene, tick, workloads[stress], &water, diagnostics, paused,
                           &player, camera.eye.y, camera_immersed, fx_result, &compositor);
        if (fx_reserved) {
            sat_result_t st;
            do {
                st = sat_fx_compositor_present(&compositor);
            } while (st == SAT_ERR_BUSY || st == SAT_ERR_IO || st == SAT_ERR_TIMEOUT);
            sat_example_must(st);
            if (water.mode != LAB_MULTIPASS) {
                sat_example_must(sat_fx_compositor_release(&compositor));
                fx_reserved = 0;
            }
        } else {
            sat_example_must(sat_app_frame_end());
        }
        lab_benchmark_submitted(tick);
        if (!paused) ++tick;
    }
}
