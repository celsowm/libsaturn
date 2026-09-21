#include "camera.h"

#include "saturn/example_util.h"

#include "p3d_config.h"

/* How much further back the camera has to sit at this angle for the whole
 * board to stay on screen.
 *
 * The maze is 224 by 200, so it is not square, and a rectangle seen corner-on
 * is wider than the same rectangle seen face-on -- 300 units across the
 * diagonal against 224 across the front. A camera distance that frames the
 * front view crops the corners of the diagonal ones.
 *
 * Holding the distance at the worst case instead would shrink the front view,
 * the one the game is mostly played in, by a quarter for the benefit of the
 * four diagonals. Since every angle is baked separately anyway, each one can
 * simply have the distance that frames it: the board stays about the same
 * size on screen whichever way it is turned, which is the thing the eye
 * actually tracks. The cost is that the camera visibly pulls back and in
 * again while turning, rather than swinging at a fixed radius.
 *
 * Both axes matter, and taking only the width is not enough: turned a
 * quarter of the way round, the maze is NARROWER than it started (200
 * against 224) but DEEPER by the same swap, and it was the depth that ran
 * off the bottom of the screen. */
static sat_fx16_t frame_scale(sat_fx16_t sin_az, sat_fx16_t cos_az) {
    const sat_fx16_t width = sat_fx16_from_int(P3D_BOARD_W);
    const sat_fx16_t depth = sat_fx16_from_int(P3D_BOARD_D);
    const sat_fx16_t abs_sin = sat_fx16_abs(sin_az);
    const sat_fx16_t abs_cos = sat_fx16_abs(cos_az);
    /* Bounding box of the rotated board, along the camera's axes. */
    const sat_fx16_t span_x =
        sat_fx16_mul(width, abs_cos) + sat_fx16_mul(depth, abs_sin);
    const sat_fx16_t span_z =
        sat_fx16_mul(width, abs_sin) + sat_fx16_mul(depth, abs_cos);
    const sat_fx16_t need_x = sat_fx16_div(span_x, width);
    const sat_fx16_t need_z = sat_fx16_div(span_z, depth);
    /* Normalised so that angle 0 comes out at exactly 1.0, which keeps the
     * straight-on view identical to the fixed camera this replaced. */
    return (need_x > need_z) ? need_x : need_z;
}

void p3d_camera_set(p3d_camera_t* camera, uint16_t angle) {
    sat_vec3_t center;
    sat_vec3_t up = {0, SAT_FX16_ONE, 0};
    const sat_fx16_t degrees = sat_fx16_from_int((int)angle * P3D_CAM_STEP_DEGREES);
    const sat_fx16_t back = sat_fx16_from_int(P3D_CAM_BACK);
    sat_fx16_t scale;

    camera->angle = angle;
    camera->sin_az = sat_sin_deg(degrees);
    camera->cos_az = sat_cos_deg(degrees);
    scale = frame_scale(camera->sin_az, camera->cos_az);

    /* Angle 0 looks straight down the -Z axis from behind the bottom edge of
     * the maze. */
    camera->eye.x = sat_fx16_from_int(P3D_BOARD_W / 2) +
                    sat_fx16_mul(sat_fx16_mul(back, scale), camera->sin_az);
    camera->eye.y = sat_fx16_mul(sat_fx16_from_int(P3D_CAM_HEIGHT), scale);
    camera->eye.z = sat_fx16_from_int(P3D_BOARD_D / 2) +
                    sat_fx16_mul(sat_fx16_mul(back, scale), camera->cos_az);

    center.x = sat_fx16_from_int(P3D_BOARD_W / 2);
    center.y = 0;
    center.z = sat_fx16_from_int(P3D_BOARD_D / 2);

    sat_example_must(sat_camera3d_init(&camera->view, &camera->eye, &center, &up,
        sat_fx16_from_int(P3D_CAM_FOV),
        sat_fx16_div(sat_fx16_from_int(P3D_SCREEN_W), sat_fx16_from_int(P3D_SCREEN_H)),
        sat_fx16_from_int(1), sat_fx16_from_int(1200)));
}

/* On the press rather than while held: a step is 22.5 degrees, and repeating
 * that every frame would spin the board eight times a second. */
int p3d_camera_update(p3d_camera_t* camera, const sat_pad_state_t* pad) {
    uint16_t next = camera->angle;

    if ((pad->pressed & SAT_PAD_L) != 0u) {
        next = (uint16_t)((camera->angle + P3D_CAM_ANGLES - 1u) % P3D_CAM_ANGLES);
    } else if ((pad->pressed & SAT_PAD_R) != 0u) {
        next = (uint16_t)((camera->angle + 1u) % P3D_CAM_ANGLES);
    }
    if (next == camera->angle) {
        return 0;
    }
    /* The baked views for every angle are already in memory; all that is
     * left is the matrix the live actors are projected through. */
    p3d_camera_set(camera, next);
    return 1;
}
