#ifndef SATURN_GRAPHICS_3D_SCENE_TEST_METRICS_H
#define SATURN_GRAPHICS_3D_SCENE_TEST_METRICS_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

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
