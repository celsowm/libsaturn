#ifndef SATURN_FOLLOW_CAMERA2D_H
#define SATURN_FOLLOW_CAMERA2D_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/collide2d.h"
#include "saturn/math2d.h"
#include "saturn/render2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Camera policy above render2d. render2d owns the final transform (sat_camera2d_t); this
 * module decides where the camera looks: it follows a target through a dead zone with
 * look-ahead and smoothing, keeps the view inside world bounds and inside temporary
 * clamps that slide in and out, adds screen shake on top, and answers the range
 * questions a game would otherwise repeat (what is visible, what to activate, what to
 * prefetch).
 *
 * Everything is in 16.16 world pixels, Y down. The camera's own state is the view
 * centre: the world point in the middle of the viewport. Shake is never part of it. It
 * is added only to the camera handed to render2d, so gameplay ranges, clamps and
 * deltas stay steady while the picture trembles.
 *
 * One step per fixed tick: sat_follow_camera2d_step reads the target (and optionally its
 * velocity), moves the centre, and fills a sat_camera2d_t ready for sat_render2d_set_camera.
 * The controller is a plain value: no allocation, no clock, no hidden state, and the same
 * inputs always produce the same camera. Zoom is left at identity and rotation at 0. */

#define SAT_FOLLOW_CAMERA2D_PIXEL_SNAP 0x01u /* round the output camera to whole pixels (no sub-pixel shimmer) */

typedef struct sat_follow_camera2d_config {
    sat_fx16_t viewport_w, viewport_h;     /* pixels, > 0 */
    sat_fx16_t dead_half_w, dead_half_h;   /* half size of the box around the view centre in which the target moves freely, >= 0 */
    sat_fx16_t shift_x, shift_y;           /* the dead zone's offset from the view centre: a positive shift_y keeps the target
                                              below the middle of the screen, which shows more of what is above it */
    sat_fx16_t look_max_x, look_max_y;     /* furthest look-ahead, px, >= 0 */
    sat_fx16_t look_gain_x, look_gain_y;   /* px of look-ahead per px/step of target velocity */
    sat_fx16_t look_ease;                  /* fraction of the remaining look-ahead closed each step, (0, ONE] */
    sat_fx16_t follow_x, follow_y;         /* fraction of the distance outside the dead zone closed each step, (0, ONE]; ONE = rigid */
    sat_fx16_t max_step_x, max_step_y;     /* fastest the view may move per step, px; 0 = unlimited */
    sat_fx16_t activation_margin;          /* px added on every side of the view for the activation range, >= 0 */
    sat_fx16_t prefetch_margin;            /* the same for the prefetch range, >= 0 */
    uint8_t predict_steps;                 /* steps of the view's current motion added on the side it is moving to, for both ranges */
    uint8_t flags;
    uint8_t reserved[2];
} sat_follow_camera2d_config_t;

/* ----- screen shake: a plain value, no task needed ----- */

typedef enum sat_shake2d_waveform {
    SAT_SHAKE2D_SINE = 0,  /* smooth: each axis follows a sine, Y a quarter turn behind X */
    SAT_SHAKE2D_NOISE = 1  /* jitter: a fresh deterministic pseudo-random value */
} sat_shake2d_waveform_t;

typedef enum sat_shake2d_envelope {
    SAT_SHAKE2D_CONSTANT = 0, /* full amplitude until the duration ends */
    SAT_SHAKE2D_LINEAR = 1,   /* fades to zero over the duration */
    SAT_SHAKE2D_STEP = 2      /* loses `decay` pixels of amplitude every step (ends at zero if duration is 0) */
} sat_shake2d_envelope_t;

typedef enum sat_shake2d_sign {
    SAT_SHAKE2D_BOTH = 0,
    SAT_SHAKE2D_POSITIVE = 1, /* offsets only push right and down */
    SAT_SHAKE2D_NEGATIVE = 2  /* offsets only push left and up */
} sat_shake2d_sign_t;

typedef struct sat_shake2d_params {
    sat_fx16_t amplitude_x, amplitude_y; /* peak offset, px; zero turns an axis off */
    uint16_t duration;                   /* steps; 0 only with STEP (until the amplitude is gone) or CONSTANT (until stopped) */
    uint8_t waveform;                    /* sat_shake2d_waveform_t */
    uint8_t envelope;                    /* sat_shake2d_envelope_t */
    uint8_t sign;                        /* sat_shake2d_sign_t */
    uint8_t update_every;                /* recompute the offset only every N steps (0 and 1 mean every step) */
    sat_fx16_t decay;                    /* STEP: amplitude lost per step, px */
    sat_angle16_t frequency;             /* SINE: phase advance per step, 65536 per turn */
    uint32_t seed;                       /* NOISE seed; the starting phase of SINE */
} sat_shake2d_params_t;

typedef struct sat_shake2d {
    sat_shake2d_params_t params;
    sat_vec2_t offset;      /* what the last step produced */
    uint32_t rng;
    sat_angle16_t phase;
    uint32_t elapsed;
    uint8_t active;
    uint8_t reserved[3];
} sat_shake2d_t;

/* SAT_ERR_INVALID_ARG for a negative amplitude or decay, an unknown enumerator, or a
 * duration of 0 with LINEAR (which would never fade). */
sat_result_t sat_shake2d_start(sat_shake2d_t* shake, const sat_shake2d_params_t* params);
void sat_shake2d_stop(sat_shake2d_t* shake); /* offset becomes zero at once */
int sat_shake2d_is_active(const sat_shake2d_t* shake);
/* Advances one step and returns the offset for it: zero once the shake is over. */
sat_vec2_t sat_shake2d_step(sat_shake2d_t* shake);

/* ----- the follow controller ----- */

typedef struct sat_camera_bounds2 {
    sat_fx16_t min_x, min_y, max_x, max_y;
} sat_camera_bounds2_t;

typedef enum sat_camera_range {
    SAT_CAMERA_RANGE_VIEW = 0,       /* what is drawn this step (includes shake) */
    SAT_CAMERA_RANGE_ACTIVATION = 1, /* the view plus activation_margin and the predicted motion */
    SAT_CAMERA_RANGE_PREFETCH = 2    /* the view plus prefetch_margin and the predicted motion */
} sat_camera_range_t;

typedef struct sat_follow_camera2d {
    sat_follow_camera2d_config_t config;
    sat_vec2_t centre;       /* view centre, world pixels; no shake */
    sat_vec2_t delta;        /* how far the centre moved in the last step (what parallax and moving platforms need) */
    sat_vec2_t look;         /* current look-ahead offset */
    sat_vec2_t shake_offset; /* what the shake added to the last output */
    sat_camera_bounds2_t world;   /* the stage's bounds for the view window (valid when has_world) */
    sat_camera_bounds2_t clamp;   /* the temporary clamp now in effect, sliding to clamp_goal */
    sat_camera_bounds2_t clamp_goal;
    sat_fx16_t clamp_speed;  /* px per step each clamp edge moves; 0 = at once */
    uint8_t has_world;
    uint8_t has_clamp;
    uint8_t clamp_releasing; /* the clamp is sliding back to the world bounds and goes away when it gets there */
    uint8_t reserved[1];
    sat_shake2d_t shake;
} sat_follow_camera2d_t;

/* A sensible starting point for a 320 x 224 view: dead zone 16 x 32 px, no look-ahead,
 * rigid follow. Every field is yours to tune. */
void sat_follow_camera2d_config_default(sat_follow_camera2d_config_t* config);

/* SAT_ERR_INVALID_ARG for a viewport that is not positive, a negative size, a margin or
 * a speed, a follow or look ease outside (0, ONE]. */
sat_result_t sat_follow_camera2d_config_validate(const sat_follow_camera2d_config_t* config);

/* Centres the view on `centre` with no look-ahead and no clamp. */
sat_result_t sat_follow_camera2d_init(sat_follow_camera2d_t* cam, const sat_follow_camera2d_config_t* config,
    sat_vec2_t centre);

/* The stage's bounds for the visible window (not for the centre): the view never shows
 * anything outside them. A window narrower than the bounds is centred on that axis.
 * NULL removes the bounds. The view is pulled inside on the next step. */
sat_result_t sat_follow_camera2d_set_bounds(sat_follow_camera2d_t* cam, const sat_camera_bounds2_t* bounds);

/* A temporary clamp for a boss room, a cutscene or a locked screen. The visible window
 * is kept inside `bounds` as well as inside the world bounds. Each edge moves towards it
 * by at most `slide_speed` px per step from wherever the previous bounds were (0 = at
 * once), so the room closes in smoothly. */
sat_result_t sat_follow_camera2d_set_clamp(sat_follow_camera2d_t* cam, const sat_camera_bounds2_t* bounds,
    sat_fx16_t slide_speed);
/* Releases the temporary clamp, sliding back to the world bounds at `slide_speed` (0 = at once).
 * Without world bounds there is nothing to slide back to, so the clamp is dropped at once. */
sat_result_t sat_follow_camera2d_clear_clamp(sat_follow_camera2d_t* cam, sat_fx16_t slide_speed);

/* Replaces the view at once, centred so `target` sits at the middle of the dead zone,
 * and clears look-ahead and delta. Use it after a teleport, a respawn or a room change. */
sat_result_t sat_follow_camera2d_snap(sat_follow_camera2d_t* cam, sat_vec2_t target);

sat_result_t sat_follow_camera2d_shake(sat_follow_camera2d_t* cam, const sat_shake2d_params_t* params);

/* One step. `velocity` (world pixels per step) drives the look-ahead and may be NULL for none.
 * `out` receives the camera to give to sat_render2d_set_camera and may be NULL. */
sat_result_t sat_follow_camera2d_step(sat_follow_camera2d_t* cam, sat_vec2_t target, const sat_vec2_t* velocity,
    sat_camera2d_t* out);

/* The camera for the current state, shake and pixel snapping included. */
sat_result_t sat_follow_camera2d_get(const sat_follow_camera2d_t* cam, sat_camera2d_t* out);

/* A world-space box for the chosen range. The view range is what is drawn (shake included);
 * the other two are built from the shake-free view. */
sat_result_t sat_follow_camera2d_range(const sat_follow_camera2d_t* cam, sat_camera_range_t range, sat_box2_t* out);

/* Whether `box` overlaps the range (edges that only touch do not overlap). */
int sat_follow_camera2d_overlaps(const sat_follow_camera2d_t* cam, sat_camera_range_t range, const sat_box2_t* box);

/* World to screen as render2d will draw it, and back. */
sat_vec2_t sat_follow_camera2d_world_to_screen(const sat_follow_camera2d_t* cam, sat_vec2_t world);
sat_vec2_t sat_follow_camera2d_screen_to_world(const sat_follow_camera2d_t* cam, sat_vec2_t screen);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_FOLLOW_CAMERA2D_H */
