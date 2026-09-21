#ifndef SATURN_FOLLOW_CAMERA3D_H
#define SATURN_FOLLOW_CAMERA3D_H

#include <stdint.h>

#include "saturn/math3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bounded chase-camera policy. The controller owns only the smoothed anchor
 * and the authored eye/target offsets; camera projection and input binding
 * remain separate responsibilities. No allocation or hidden state. */
typedef struct sat_follow_camera3d {
    sat_vec3_t anchor;
    sat_vec3_t eye_offset;
    sat_vec3_t target_offset;
    uint8_t follow_x;
    uint8_t follow_y;
    uint8_t follow_z;
    uint8_t reserved;
} sat_follow_camera3d_t;

sat_result_t sat_follow_camera3d_init(
    sat_follow_camera3d_t* controller,
    const sat_vec3_t* anchor,
    uint8_t follow_x,
    uint8_t follow_y,
    uint8_t follow_z);

sat_result_t sat_follow_camera3d_set_offsets(
    sat_follow_camera3d_t* controller,
    const sat_vec3_t* eye_offset,
    const sat_vec3_t* target_offset);

/* Advances the smoothed anchor toward desired. When snap is non-zero the
 * anchor is replaced exactly, which is useful after a fall/reset. Outputs the
 * camera eye and look target but does not build a projection matrix. */
sat_result_t sat_follow_camera3d_step(
    sat_follow_camera3d_t* controller,
    const sat_vec3_t* desired,
    uint8_t snap,
    sat_vec3_t* out_eye,
    sat_vec3_t* out_target);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_FOLLOW_CAMERA3D_H */
