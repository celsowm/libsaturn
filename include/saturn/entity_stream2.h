#ifndef SATURN_ENTITY_STREAM2_H
#define SATURN_ENTITY_STREAM2_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/collide2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Activation of static level entities by region. A stage has thousands of placed things
 * (rings, enemies, springs, decorations) that sit still until the camera comes near; this
 * module keeps them as immutable descriptors, and tells the game when one should become a
 * live object and when it should go back to sleep. It is not an ECS: it owns no live
 * objects. The game owns the pool, creates the object in the activate callback and frees it
 * in the deactivate callback, and keeps any state that has to outlive the object (a
 * collected ring, a defeated boss) in its own data, or retires the descriptor here.
 *
 * The descriptors are laid out offline (tools build the index in Phase 10): sorted by the
 * region they sit in, regions in row-major order, with a start-index table in the manner
 * of a compressed-sparse-row matrix. Finding the descriptors near the camera is then a walk
 * over a few regions, never a scan of the level; nothing is built at run time and nothing
 * is written to the descriptors. What is active is one bit per descriptor, in storage the
 * caller owns.
 *
 *   region r holds the descriptors  region_start[r] .. region_start[r + 1] - 1
 *   region (col, row) is r = row * region_cols + col
 *   a descriptor at (x, y), relative to the index origin, is in col = x >> region_shift,
 *   row = y >> region_shift
 *
 * One update takes the activation box (world pixels, 16.16; for the follow camera this is
 * its ACTIVATION range, which already holds the prediction margin) and does, in order:
 *
 *   1. every active descriptor outside the box grown by `hysteresis` is deactivated, in
 *      ascending index order;
 *   2. every inactive, not retired descriptor inside the box is activated, in ascending
 *      index order.
 *
 * A descriptor that stays inside the grown box does not flicker when the camera hovers at
 * the edge of the box. A callback never fires twice for the same crossing, and a camera
 * that jumps across any number of regions in one frame is handled in that one update: the
 * far descriptors are released, the new neighbourhood is activated. Deactivation runs
 * first so that the pool slots it frees are there for the activations.
 *
 * When the pool is full the activate callback says so and the update stops, keeping the
 * order: the descriptors not yet activated are tried again, first to last, on the next
 * update. */

typedef struct sat_entity_desc2 {
    uint16_t x, y;   /* pixels from the index origin, 0..65535 */
    uint16_t kind;   /* game-defined */
    uint16_t data;   /* game-defined; anything bigger lives in a game array indexed by descriptor */
} sat_entity_desc2_t;

typedef struct sat_entity_index2 {
    const sat_entity_desc2_t* descs;
    const uint16_t* region_start; /* region_cols * region_rows + 1 entries, non-decreasing, first 0, last desc_count */
    int32_t origin_x, origin_y;   /* world pixels of the grid's top-left corner */
    uint16_t desc_count;
    uint16_t region_cols, region_rows;
    uint8_t region_shift;         /* log2 of the region edge in pixels, 3..15 */
    uint8_t reserved;
} sat_entity_index2_t;

/* Checks the index against its own rules: table shape and monotonicity, and that every
 * descriptor really lies in the region that lists it. SAT_ERR_INVALID_ARG otherwise. The
 * check is O(descriptors); sat_entity_stream2_init runs it once. */
sat_result_t sat_entity_index2_validate(const sat_entity_index2_t* index);

/* Bytes of the index tables a plan has to charge (descriptors plus region table). */
uint32_t sat_entity_index2_bytes(uint32_t desc_count, uint32_t region_count);

typedef enum sat_entity_activate_result {
    SAT_ENTITY_ACTIVATED = 0, /* the object exists now */
    SAT_ENTITY_DEFER = 1,     /* no room: stop this update, try this descriptor first next time */
    SAT_ENTITY_DECLINE = 2    /* never activate this descriptor (it is retired) */
} sat_entity_activate_result_t;

typedef sat_entity_activate_result_t (*sat_entity_activate_fn)(void* user, uint32_t index, const sat_entity_desc2_t* desc);
typedef void (*sat_entity_deactivate_fn)(void* user, uint32_t index, const sat_entity_desc2_t* desc);

typedef struct sat_entity_stream2_config {
    const sat_entity_index2_t* index;
    uint32_t* state;              /* caller-owned: sat_entity_stream2_state_words() words, any contents */
    uint32_t state_words;
    sat_fx16_t hysteresis;        /* px added on every side of the box before an active descriptor is released, >= 0 */
    sat_entity_activate_fn activate;     /* required */
    sat_entity_deactivate_fn deactivate; /* optional (a game that only spawns can leave it out) */
    void* user;
} sat_entity_stream2_config_t;

typedef struct sat_entity_stream2_stats {
    uint32_t updates;
    uint32_t activations;
    uint32_t deactivations;
    uint32_t deferrals;    /* updates stopped by a full pool */
    uint32_t declines;
    uint32_t peak_active;
} sat_entity_stream2_stats_t;

typedef struct sat_entity_stream2_result {
    uint32_t activated;   /* by this update */
    uint32_t deactivated;
    uint8_t deferred;     /* 1 when a full pool stopped the activations early */
    uint8_t reserved[3];
} sat_entity_stream2_result_t;

typedef struct sat_entity_stream2 {
    sat_entity_index2_t index;
    uint32_t* active;   /* one bit per descriptor */
    uint32_t* retired;  /* one bit per descriptor */
    uint32_t bit_words; /* words of each bitset */
    uint32_t active_count;
    sat_fx16_t hysteresis;
    sat_entity_activate_fn activate;
    sat_entity_deactivate_fn deactivate;
    void* user;
    sat_entity_stream2_stats_t stats;
} sat_entity_stream2_t;

/* Words of caller-owned state for `desc_count` descriptors: the active and the retired
 * bitsets together (4 bytes each). */
uint32_t sat_entity_stream2_state_words(uint32_t desc_count);

/* SAT_ERR_INVALID_ARG for a missing pointer or callback, a state buffer that is too small,
 * a negative hysteresis or an inconsistent index. Everything starts inactive and not retired. */
sat_result_t sat_entity_stream2_init(sat_entity_stream2_t* stream, const sat_entity_stream2_config_t* config);

/* One update with the box that should be alive (see above). SAT_ERR_INVALID_ARG for a
 * missing or inverted box. `out` may be NULL. */
sat_result_t sat_entity_stream2_update(sat_entity_stream2_t* stream, const sat_box2_t* activation, sat_entity_stream2_result_t* out);

int sat_entity_stream2_is_active(const sat_entity_stream2_t* stream, uint32_t index);
int sat_entity_stream2_is_retired(const sat_entity_stream2_t* stream, uint32_t index);
uint32_t sat_entity_stream2_active_count(const sat_entity_stream2_t* stream);

/* The game released the object itself (it was destroyed, say). The descriptor is inactive
 * without a deactivate callback and activates again the next time it is inside the box. */
sat_result_t sat_entity_stream2_release(sat_entity_stream2_t* stream, uint32_t index);
/* Release it and never activate it again (a collected ring). No callback. */
sat_result_t sat_entity_stream2_retire(sat_entity_stream2_t* stream, uint32_t index);
/* Let a retired descriptor activate again. */
sat_result_t sat_entity_stream2_unretire(sat_entity_stream2_t* stream, uint32_t index);
/* Everything inactive and nothing retired, without callbacks: a stage restart. The game frees its
 * own pool. */
void sat_entity_stream2_reset(sat_entity_stream2_t* stream);

sat_entity_stream2_stats_t sat_entity_stream2_stats(const sat_entity_stream2_t* stream);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_ENTITY_STREAM2_H */
