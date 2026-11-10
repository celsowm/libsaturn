#ifndef WORLD_LOD3D_DEMO_H
#define WORLD_LOD3D_DEMO_H

#include <stdint.h>

/* Work RAM High telemetry read by harness/tests/test_world_lod3d_demo.py.
 * Camera coordinates are signed 16.16 values stored in uint32_t words. */
typedef struct world_lod3d_demo_telemetry {
    uint32_t magic;
    uint32_t frames;
    uint32_t planner_status;
    uint32_t wanted_lod0, wanted_lod1, wanted_lod2;
    uint32_t selected_lod0, selected_lod1, selected_lod2;
    uint32_t dropped;
    uint32_t faces;
    uint32_t commands;
    uint32_t face_budget;
    uint32_t command_budget;
    uint32_t considered;
    uint32_t frustum_rejected;
    uint32_t no_resident;
    uint32_t governor_enabled;
    uint32_t governor_samples;
    uint32_t governor_transitions;
    uint32_t faded_objects;
    uint32_t fade_enabled;
    uint32_t elapsed_fields;
    uint32_t field_hist_1, field_hist_2, field_hist_3, field_hist_4;
    uint32_t field_hist_gt4;
    uint32_t scene_result;
    uint32_t cpu_frt_ticks;
    uint32_t cpu_frt_sum;
    uint32_t camera_x_fx;
    uint32_t camera_z_fx;
    uint32_t yaw_degrees;
    uint32_t pitch_degrees;
    uint32_t camera_distance_fx;
    uint32_t view_generation;
} world_lod3d_demo_telemetry_t;

#define WORLD_LOD3D_DEMO_MAGIC 0x574C4F44u /* "WLOD" */
extern volatile world_lod3d_demo_telemetry_t g_world_lod3d_demo;

#endif
