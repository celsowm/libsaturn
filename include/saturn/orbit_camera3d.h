#ifndef SATURN_ORBIT_CAMERA3D_H
#define SATURN_ORBIT_CAMERA3D_H

#include <stdint.h>
#include "saturn/input.h"
#include "saturn/math3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Model-viewer camera: shared fixed-point orbit, bounded zoom, perspective,
 * and gamepad controls. All geometry and state are caller-owned. The target
 * is derived from bounds, not assumed to be the model's bind-pose origin.
 * For animation use UNION bounds across the full clip, not one pose. */
typedef struct sat_orbit_camera3d_fit {
    sat_fx16_t min_extent;            /* reject a zero-size target safely */
    sat_fx16_t min_distance_floor;    /* optional absolute zoom-in floor */
    sat_fx16_t initial_distance_factor;
    sat_fx16_t min_distance_factor;
    sat_fx16_t max_distance_factor;
    sat_fx16_t near_plane_factor;     /* 0 disables size-relative near plane */
    sat_fx16_t near_plane_floor;      /* strictly positive camera near guard */
    sat_fx16_t fov_y;
    sat_fx16_t aspect;
    sat_fx16_t far_z;
    int16_t pitch_min_deg;
    int16_t pitch_max_deg;
} sat_orbit_camera3d_fit_t;

typedef struct sat_orbit_camera3d {
    sat_vec3_t target;
    sat_vec3_t eye;
    sat_mat4_t view_proj;
    sat_fx16_t distance;
    sat_fx16_t default_distance;
    sat_fx16_t min_distance;
    sat_fx16_t max_distance;
    sat_fx16_t near_z;
    sat_fx16_t far_z;
    sat_fx16_t fov_y;
    sat_fx16_t aspect;
    int32_t yaw_deg;
    int32_t pitch_deg;
    int16_t pitch_min_deg;
    int16_t pitch_max_deg;
    uint8_t auto_orbit;
} sat_orbit_camera3d_t;

/* Fits the entire supplied AABB with configurable size-relative policies.
 * Rejects malformed bounds, projection and numerical overflow WITHOUT
 * modifying the output camera. A large absolute zoom floor automatically
 * raises maximum zoom distance rather than leaving min > max. */
sat_result_t sat_orbit_camera3d_fit_bounds(
    sat_orbit_camera3d_t* out,
    const sat_vec3_t* min_xyz, const sat_vec3_t* max_xyz,
    const sat_orbit_camera3d_fit_t* policy
);

/* Reset to yaw=0, pitch=10 (clamped) and the originally fitted distance. */
sat_result_t sat_orbit_camera3d_reset(sat_orbit_camera3d_t* camera);

/* Rebuild eye and projection from orbit state without gamepad input. */
sat_result_t sat_orbit_camera3d_update(sat_orbit_camera3d_t* camera);

/* Original Saturn viewer controls: LEFT/RIGHT yaw 2deg, UP/DOWN pitch
 * 2deg, L/R multiplicative zoom at 1/40 step, B reset; auto-toggle button
 * is chosen by each game (A or C) and adds 1 degree/frame. START/HUD and
 * animation pause/reset belong to the application, NOT this camera.
 * Calling with a null pad or a button mask outside a single pad bit fails. */
sat_result_t sat_orbit_camera3d_apply_pad(
    sat_orbit_camera3d_t* camera,
    const sat_pad_state_t* pad,
    uint16_t auto_toggle_button
);

#ifdef __cplusplus
}
#endif
#endif /* SATURN_ORBIT_CAMERA3D_H */
