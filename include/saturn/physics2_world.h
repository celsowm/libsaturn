#ifndef SATURN_PHYSICS2_WORLD_H
#define SATURN_PHYSICS2_WORLD_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/collide2d.h"
#include "saturn/spatial.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Optional 2D world for colliders that are not terrain tiles: platforms, doors,
 * hazards, pickups, trigger volumes. It gives them a stable identity, tracks how far
 * the moving ones travelled this tick, answers "what supports this foot" with that
 * motion attached, and reports sensor enter/stay/leave events.
 *
 * Collider classes:
 *   - STATIC: never moves, solid, may be one-way;
 *   - KINEMATIC: moved by the game with sat_physics2_set_center, solid, may be one-way,
 *     carries whatever stands on it;
 *   - SENSOR: a volume that solid colliders are reported against; never solid itself.
 *
 * Every collider is an axis-aligned box (centre and half extents, 16.16 world pixels,
 * Y down). Boxes only touch when their edges meet; they overlap only when they share
 * area, as in saturn/collide2d.h.
 *
 * Handles. A collider is named by a sat_collider2_t. A handle stays valid until its
 * collider is removed; a removed collider's handle is never valid again, even when its
 * slot is reused (65535 reuses of one slot later the generation wraps). Every function
 * that takes a handle answers SAT_ERR_NOT_FOUND for a stale one, so a game that stored a
 * support or a target can detect that it is gone without a raw pointer.
 *
 * One tick, in the order a game uses it:
 *   1. move kinematic colliders and sensors (sat_physics2_set_center);
 *   2. resolve characters: find_support, then carry them by the support's delta;
 *   3. sat_physics2_step: sensor events, then the tick is committed (every collider's
 *      "previous" position becomes its current one, so the next delta starts at zero).
 * sat_physics2_delta is the motion since the last step; reading it before step is what
 * makes carry work.
 *
 * Broadphase. The world indexes its colliders in a caller-owned sat_spatial_t using the
 * boxes they swept this tick, so a fast collider is found even if it moved past a sensor
 * between two steps. Queries and steps give identical results for the same inputs; the
 * order of every result list is by collider handle, never by hash or insertion luck.
 *
 * Nothing here allocates, touches hardware or depends on the renderer, Terrain2 or
 * Character2. A character standing on a platform is composed by the game from
 * find_support + carry + launch_velocity; see those functions. */

typedef uint32_t sat_collider2_t;
#define SAT_COLLIDER2_NONE 0u /* never a valid handle */

typedef enum sat_collider2_kind {
    SAT_COLLIDER2_STATIC = 1,
    SAT_COLLIDER2_KINEMATIC = 2,
    SAT_COLLIDER2_SENSOR = 3
} sat_collider2_kind_t;

#define SAT_COLLIDER2_KIND_BIT(kind) (1u << (kind)) /* for a query's kind_mask */
#define SAT_COLLIDER2_SOLID_KINDS (SAT_COLLIDER2_KIND_BIT(SAT_COLLIDER2_STATIC) | SAT_COLLIDER2_KIND_BIT(SAT_COLLIDER2_KINEMATIC))
#define SAT_COLLIDER2_ALL_KINDS (SAT_COLLIDER2_SOLID_KINDS | SAT_COLLIDER2_KIND_BIT(SAT_COLLIDER2_SENSOR))

#define SAT_COLLIDER2_ONE_WAY 0x01u /* solid only on the face named by one_way_face */

typedef struct sat_collider2_desc {
    sat_collider2_kind_t kind;
    sat_box2_t box;          /* centre and half extents */
    uint16_t category;       /* what this collider is, as bits of the game's choosing */
    uint16_t mask;           /* sensors only: the categories of solid colliders it reports */
    uint8_t flags;           /* SAT_COLLIDER2_ONE_WAY */
    uint8_t one_way_face;    /* one-way only: direction of the solid face's outward normal,
                                0 +X, 1 +Y, 2 -X, 3 -Y. A platform you land on from above has
                                its solid face on top, outward normal -Y: 3 */
    sat_fx16_t launch_scale; /* fraction of the collider's motion handed to something that
                                leaves it (see sat_physics2_launch_velocity); ONE = all of it */
    uint32_t user;           /* returned untouched; typically the game's object index */
} sat_collider2_desc_t;

/* Internal slot; declared so the caller can own the storage. */
typedef struct sat_collider2_slot {
    sat_collider2_desc_t desc;
    sat_vec2_t previous;     /* centre at the last committed step */
    uint16_t generation;     /* 0 is never a live generation */
    uint16_t next_free;      /* free list link while the slot is unused */
    uint8_t alive;
    uint8_t reserved[3];
} sat_collider2_slot_t;

typedef struct sat_physics2_pair {
    sat_collider2_t sensor;
    sat_collider2_t other;
    uint8_t state;           /* internal */
    uint8_t reserved[3];
} sat_physics2_pair_t;

typedef struct sat_physics2_storage {
    sat_collider2_slot_t* slots;
    uint16_t slot_cap;           /* 1..65534 */
    sat_physics2_pair_t* pairs;       /* overlap pairs alive after the last step */
    sat_physics2_pair_t* next_pairs;  /* same capacity: the pairs being built by a step */
    uint16_t pair_cap;
    uint16_t* candidates;        /* slot_cap entries of query scratch */
    sat_spatial_t* spatial;      /* initialised by the caller with item_cap >= slot_cap */
} sat_physics2_storage_t;

typedef struct sat_physics2_world {
    sat_physics2_storage_t storage;
    uint16_t count;              /* live colliders */
    uint16_t pair_count;
    uint16_t free_head;          /* first unused slot, or SAT_PHYSICS2_NO_SLOT */
    uint8_t index_dirty;         /* the spatial index is stale */
    uint8_t emit_stay;           /* report STAY events every step */
    uint8_t index_overflow;      /* the last rebuild did not fit */
    uint8_t reserved[3];
} sat_physics2_world_t;

#define SAT_PHYSICS2_NO_SLOT ((uint16_t)0xFFFFu)

/* Bytes of caller storage a world needs: slots, both pair arrays and the candidate
 * scratch. The sat_spatial_t arrays are sized by the caller (about four entries per
 * collider is plenty when cells are about as large as the largest collider). */
uint32_t sat_physics2_requirements(uint16_t slot_cap, uint16_t pair_cap);

/* Returns SAT_ERR_INVALID_ARG for missing storage, a zero capacity, or a spatial index
 * that cannot hold slot_cap colliders. */
sat_result_t sat_physics2_world_init(sat_physics2_world_t* world, const sat_physics2_storage_t* storage);

/* Removes every collider and pair; handles from before stay invalid. */
void sat_physics2_world_reset(sat_physics2_world_t* world);

/* STAY events are off by default; they cost a write per overlapping pair per step. */
void sat_physics2_set_emit_stay(sat_physics2_world_t* world, int enabled);

/* kind and box set, category 1, mask all bits, no flags, launch_scale ONE. */
void sat_collider2_desc_init(sat_collider2_desc_t* desc, sat_collider2_kind_t kind, const sat_box2_t* box);

sat_result_t sat_physics2_add(sat_physics2_world_t* world, const sat_collider2_desc_t* desc, sat_collider2_t* out);

/* SAT_ERR_NOT_FOUND for a stale handle. Pairs that involve the collider produce LEAVE
 * events (flagged REMOVED) at the next step. */
sat_result_t sat_physics2_remove(sat_physics2_world_t* world, sat_collider2_t id);

int sat_physics2_is_valid(const sat_physics2_world_t* world, sat_collider2_t id);
sat_result_t sat_physics2_get(const sat_physics2_world_t* world, sat_collider2_t id, sat_collider2_desc_t* out);

/* Kinematic colliders and sensors only (SAT_ERR_INVALID_ARG for a static one). The
 * previous position is kept, so the motion since the last step is available as delta. */
sat_result_t sat_physics2_set_center(sat_physics2_world_t* world, sat_collider2_t id, sat_vec2_t center);
sat_result_t sat_physics2_set_box(sat_physics2_world_t* world, sat_collider2_t id, const sat_box2_t* box);

/* Motion since the last step, in world pixels. Zero for a static collider. */
sat_result_t sat_physics2_delta(const sat_physics2_world_t* world, sat_collider2_t id, sat_vec2_t* out);

/* Colliders overlapping `box`, ordered by handle. `kind_mask` is a combination of
 * SAT_COLLIDER2_KIND_BIT, `category_mask` selects colliders whose category shares a bit.
 * SAT_ERR_CAPACITY when more matched than `cap` (the first `cap` are returned). */
sat_result_t sat_physics2_query(sat_physics2_world_t* world, const sat_box2_t* box, uint8_t kind_mask,
    uint16_t category_mask, sat_collider2_t* out, uint16_t cap, uint16_t* count);

/* What a foot rests on. The probe runs from `origin` along `down_quadrant` (0 +X, 1 +Y,
 * 2 -X, 3 -Y; gravity normally pulls along +Y) across `half_width` either side of the
 * origin. It finds the nearest solid face whose distance from the origin lies in
 * [-embed, reach) pixels: reach is how far below the feet to look, embed how far the feet
 * may already sit inside the face. A one-way collider only counts when it is probed onto
 * its solid face. Sensors never support. */
typedef struct sat_physics2_support_query {
    sat_vec2_t origin;
    sat_fx16_t half_width;
    uint8_t down_quadrant;
    uint8_t reach;
    uint8_t embed;
    uint8_t reserved;
    uint16_t category_mask;
} sat_physics2_support_query_t;

typedef struct sat_physics2_support {
    sat_collider2_t id;
    sat_vec2_t point;   /* the face under the origin */
    sat_vec2_t normal;  /* unit, away from the collider */
    sat_vec2_t delta;   /* the collider's motion since the last step */
    sat_fx16_t distance; /* from the origin to the face; negative when the origin is inside it */
} sat_physics2_support_t;

/* SAT_ERR_NOT_FOUND when nothing supports the origin. Support is meant to be re-tested
 * every tick: a platform that moved away simply stops being found. */
sat_result_t sat_physics2_find_support(sat_physics2_world_t* world, const sat_physics2_support_query_t* query,
    sat_physics2_support_t* out);

/* Adds the collider's motion since the last step to `position`. SAT_ERR_NOT_FOUND for a
 * stale handle: the support was removed, so the character should detach. */
sat_result_t sat_physics2_carry(const sat_physics2_world_t* world, sat_collider2_t id, sat_vec2_t* position);

/* The velocity (world pixels per step) a character keeps from the collider when it
 * leaves it: the collider's motion since the last step times its launch_scale. */
sat_result_t sat_physics2_launch_velocity(const sat_physics2_world_t* world, sat_collider2_t id, sat_vec2_t* out);

typedef enum sat_physics2_event_type {
    SAT_PHYSICS2_EVENT_ENTER = 1,
    SAT_PHYSICS2_EVENT_STAY = 2,
    SAT_PHYSICS2_EVENT_LEAVE = 3
} sat_physics2_event_type_t;

#define SAT_PHYSICS2_EVENT_CROSSING 0x01u /* ENTER and LEAVE of one step: it passed through between steps */
#define SAT_PHYSICS2_EVENT_REMOVED 0x02u  /* LEAVE because the sensor or the other collider was removed */

typedef struct sat_physics2_event {
    sat_collider2_t sensor;
    sat_collider2_t other;
    uint8_t type;   /* sat_physics2_event_type_t */
    uint8_t flags;
    uint16_t reserved;
} sat_physics2_event_t;

typedef struct sat_physics2_step_result {
    uint16_t events;          /* written to the buffer */
    uint16_t events_dropped;  /* did not fit; pair state is still updated */
    uint16_t pairs_dropped;   /* more overlapping pairs than pair_cap; they are not tracked */
} sat_physics2_step_result_t;

/* Reports, for every sensor in handle order and for each solid collider in handle order,
 * ENTER when it begins to overlap, LEAVE when it stops, and STAY while it continues (if
 * enabled). A solid collider that moved through a sensor between two steps without ever
 * overlapping it at a step reports ENTER then LEAVE with SAT_PHYSICS2_EVENT_CROSSING.
 * Then commits the tick. `events` may be NULL with cap 0 to just advance. Returns
 * SAT_ERR_CAPACITY when events or pairs were dropped, or when the spatial index could not
 * hold every collider; SAT_OK otherwise. */
sat_result_t sat_physics2_step(sat_physics2_world_t* world, sat_physics2_event_t* events, uint16_t cap,
    sat_physics2_step_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_PHYSICS2_WORLD_H */
