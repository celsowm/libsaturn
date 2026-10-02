#ifndef SATURN_CHARACTER2_H
#define SATURN_CHARACTER2_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/collide2d.h"
#include "saturn/math2d.h"
#include "saturn/terrain2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Surface controller for a character that walks on oriented terrain.
 *
 * Character2 owns support tracking and motion resolution only: it keeps the
 * character on the surface (floor, slope, wall, ceiling), converts between
 * surface-relative and world-relative motion, and stops it against walls and
 * ceilings. Acceleration, slope forces, jumping, rolling, abilities, damage and
 * animation belong to the game: it edits `ground_speed` (supported) or
 * `air_velocity` (airborne) before each step, and reads the returned events.
 *
 * "Supported" means the character has valid support, not that it stands on a
 * world-up floor: a character running along a wall or a ceiling is supported.
 *
 * Position. `position` is the contact point of the feet, in 16.16 world pixels,
 * on the character's own centre line. While supported it lies exactly on the
 * surface boundary along the support axis; the sub-pixel part along the surface
 * is kept. All sensors work on whole pixels (see saturn/terrain2.h).
 *
 * Sensors, in the frame of the current support (floor mode: down is +Y):
 *   - two foot sensors `foot_half_width` pixels either side of the centre line,
 *     starting `step_up` pixels above the feet and reaching `snap_down` below;
 *     the one that finds the nearest surface decides the support and its angle;
 *   - one wall sensor `wall_height` pixels above the feet, looking along the
 *     direction of travel, whose body edge sits `wall_radius` pixels ahead;
 *   - when airborne, two head sensors `head_height` pixels above the feet.
 * Keep foot_half_width <= wall_radius and step_up >= foot_half_width so a 45 degree
 * slope is not mistaken for a wall; keep wall_height >= wall_radius so the wall
 * sensor does not read the slope the character is climbing.
 *
 * The support frame follows the surface angle in four modes with the usual
 * overlap: angles 224..32 are floor mode, 33..95 and 161..223 are the walls,
 * 96..160 is ceiling mode.
 *
 * Fast movement is cut into segments of at most `segment_px` pixels, each fully
 * resolved, so nothing tunnels through terrain at up to
 * segment_px * max_segments pixels per step.
 *
 * Everything here is deterministic and allocates nothing. The map is borrowed
 * and read-only. Nothing depends on the renderer or on Physics2.
 */

#define SAT_CHARACTER2_SUPPORTED 0x01u /* character.flags */

#define SAT_CHARACTER2_NO_SUPPORT 0xFFFFFFFFu

/* step events (sat_character2_result.events) */
#define SAT_CHARACTER2_EVENT_LANDED 0x01u       /* airborne -> supported */
#define SAT_CHARACTER2_EVENT_DETACHED 0x02u     /* supported -> airborne */
#define SAT_CHARACTER2_EVENT_HIT_WALL 0x04u     /* blocked while moving sideways or along the surface */
#define SAT_CHARACTER2_EVENT_HIT_CEILING 0x08u  /* head blocked while moving against gravity */
#define SAT_CHARACTER2_EVENT_SLIPPED 0x10u      /* detached because speed fell below the steep limit */

typedef struct sat_character2 {
    sat_vec2_t position;        /* feet contact point, 16.16 world pixels */
    sat_vec2_t air_velocity;    /* world pixels per step; meaningful while airborne */
    sat_fx16_t ground_speed;    /* pixels per step along support_tangent; meaningful while supported */
    sat_vec2_t support_normal;  /* unit, away from the surface */
    sat_vec2_t support_tangent; /* unit, travel direction with the solid on the right */
    uint32_t support_id;        /* terrain collider id of the support, or SAT_CHARACTER2_NO_SUPPORT */
    uint8_t support_angle;      /* sat_angle_t of the support surface */
    uint8_t flags;              /* SAT_CHARACTER2_SUPPORTED */
    uint8_t layer;              /* terrain layer used by every query; change it at any time */
    uint8_t reserved;
} sat_character2_t;

typedef struct sat_character2_config {
    uint8_t foot_half_width;  /* px, foot sensors either side of the centre line */
    uint8_t wall_radius;      /* px from the centre line to a wall contact */
    uint8_t wall_height;      /* px above the feet of the wall sensor */
    uint8_t head_height;      /* px from the feet to the top of the head */
    uint8_t step_up;          /* tallest step climbed while supported, px */
    uint8_t snap_down;        /* longest drop followed while supported, px; a deeper one detaches */
    uint8_t max_angle_step;   /* largest angle change between successive supports (sat_angle_t steps) */
    uint8_t steep_angle;      /* angle from level above which min_steep_speed applies */
    sat_fx16_t min_steep_speed; /* |ground_speed| needed to stay on a steep or inverted surface */
    uint8_t ceiling_attach;   /* 0 = a head bump only stops the rise. Otherwise a slanted ceiling at
                                 least this many angle steps away from flat attaches the character
                                 to it (it keeps its speed along the surface) */
    uint8_t gravity_quadrant; /* direction gravity pulls: 0 +X, 1 +Y (normal), 2 -X, 3 -Y (inverted) */
    uint8_t segment_px;       /* longest movement segment, 1..8 */
    uint8_t max_segments;     /* 1..32 */
    uint8_t category_mask;    /* terrain category mask for every query */
    uint8_t ignore_flags;     /* terrain flags to skip in every query */
    uint8_t reserved;
} sat_character2_config_t;

typedef struct sat_character2_result {
    uint32_t events;           /* SAT_CHARACTER2_EVENT_* bits raised during the step */
    sat_vec2_t velocity_before; /* world velocity when the step began; the game can use it to
                                   replace the default landing or wall conversion */
    uint8_t surface_angle;     /* support angle after the step (valid while supported) */
    uint8_t reserved[3];
} sat_character2_result_t;

/* A character at (x, y), airborne and at rest, layer 0. */
void sat_character2_init(sat_character2_t* ch, sat_fx16_t x, sat_fx16_t y);

/* Example values for a small character (about 10 px wide, 24 px tall). They are a
 * starting point, not a recommendation: every field is yours to tune. */
void sat_character2_config_default(sat_character2_config_t* cfg);

/* SAT_OK, or SAT_ERR_INVALID_ARG naming nothing more than "this configuration
 * cannot work": a field out of range or a sensor reach above the probe limit. */
sat_result_t sat_character2_config_validate(const sat_character2_config_t* cfg);

int sat_character2_is_supported(const sat_character2_t* ch);

/* Velocity in world pixels per step: tangent * ground_speed while supported,
 * air_velocity otherwise. */
sat_vec2_t sat_character2_world_velocity(const sat_character2_t* ch);

/* Looks for support under the feet (within step_up above and snap_down below)
 * and attaches to it if found. Use it to place a character on the ground at
 * spawn. Returns SAT_ERR_NOT_FOUND when there is no support in reach. */
sat_result_t sat_character2_attach(sat_character2_t* ch, const sat_character2_config_t* cfg,
    const sat_terrain_map2_t* map);

/* Leaves the surface keeping momentum: air_velocity becomes the current world
 * velocity and the character is airborne. A jump is detach plus a velocity change
 * made by the game. No-op while airborne. */
void sat_character2_detach(sat_character2_t* ch);

/* Advances the character by one step. `result` may be NULL. Returns
 * SAT_ERR_INVALID_ARG for a missing/invalid map, layer or configuration. */
sat_result_t sat_character2_step(sat_character2_t* ch, const sat_character2_config_t* cfg,
    const sat_terrain_map2_t* map, sat_character2_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_CHARACTER2_H */
