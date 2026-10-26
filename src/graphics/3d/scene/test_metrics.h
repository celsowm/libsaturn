#ifndef SATURN_GRAPHICS_3D_SCENE_TEST_METRICS_H
#define SATURN_GRAPHICS_3D_SCENE_TEST_METRICS_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* Diagnostic-only, single executor geometry job at a time. Read only after
 * successful completion; all fields are written through uncached aliases.
 * Worker intervals use its local 16-bit FRT, never Master timestamps. */
typedef struct sat_scene3d_worker_profile {
    uint32_t source_ticks, geometry_ticks, output_ticks, total_ticks;
    uint32_t ccr, timer_control;
} sat_scene3d_worker_profile_t;
void sat_scene3d_test_worker_profile(sat_scene3d_worker_profile_t* out);
uint32_t sat_scene3d_test_painter_ticks(void);
uint32_t sat_scene3d_test_emit_ticks(void);
uint32_t sat_scene3d_test_input_publish_ticks(void);
uint32_t sat_vdp1_test_scene_command_hash(void);
uint32_t sat_vdp1_test_scene_command_count(void);
uint32_t sat_vdp1_test_scene_command_capacity(void);
/* Submits/texture writes that found the VDP1 still drawing; and gave up. */
uint32_t sat_vdp1_test_draw_waits(void);
uint32_t sat_vdp1_test_draw_timeouts(void);

#if defined(__cplusplus)
}
#endif

#endif
