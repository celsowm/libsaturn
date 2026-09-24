#ifndef SATURN_VIEW_CACHE_H
#define SATURN_VIEW_CACHE_H

#include <stdint.h>

#include "saturn/math3d.h"
#include "saturn/render3d.h"
#include "saturn/scene3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Immutable, caller-owned finite camera-view cache. Entries are copied into
 * the cache during bake; runtime replay never projects or sorts them. A
 * generation change invalidates every view before a new bake. The application
 * may use tag for occupancy metadata. */
typedef struct sat_view_cache_item {
    sat_quad2_t quad;
    uint32_t depth;
    uint16_t color;
    uint16_t tag;
    /* Populated only by append_world: true linear 16.16 camera depth,
     * distinct from the legacy arbitrary uint32_t bake-order key. */
    sat_fx16_t camera_depth;
    uint8_t camera_depth_valid;
} sat_view_cache_item_t;

typedef struct sat_view_cache_stats {
    uint32_t baked_entries;
    uint32_t cache_hits;
    uint16_t views_ready;
} sat_view_cache_stats_t;

/* Optional caller-provided slot for EXACT camera/viewport matching. Fixed
 * 16.16 values and all matrix elements are compared without hashes/collisions.
 * Each slot is copied on begin_camera, never holds a camera pointer. */
typedef struct sat_view_cache_camera {
    sat_camera3d_t camera;
    sat_fx16_t near_depth;
    uint16_t width, height;
    uint8_t valid;
} sat_view_cache_camera_t;

typedef struct sat_view_cache {
    sat_view_cache_item_t* storage;
    sat_view_cache_camera_t* cameras; /* NULL for the independent legacy path */
    uint16_t* counts;
    uint16_t view_count;
    uint16_t capacity_per_view;
    uint16_t current_view;
    uint32_t generation;
    uint32_t baked_entries;
    uint32_t cache_hits;
    uint8_t active;
} sat_view_cache_t;

sat_result_t sat_view_cache_init(sat_view_cache_t* cache,
    sat_view_cache_item_t* storage, uint16_t* counts,
    uint16_t view_count, uint16_t capacity_per_view);
/* Opt-in camera safety; the supplied array has at least view_count slots and
 * lives as long as cache. Binding clears all existing views (no stale reads).
 * Legacy begin/view remain available when camera matching is unwanted. */
sat_result_t sat_view_cache_bind_cameras(
    sat_view_cache_t* cache, sat_view_cache_camera_t* slots,
    uint16_t slot_count);

/* Begin a view bake with its exact current camera and native viewport.
 * Every appended quad must use this projection and camera depth. */
sat_result_t sat_view_cache_begin_camera(
    sat_view_cache_t* cache, uint16_t view,
    const sat_camera3d_t* camera, sat_fx16_t near_depth,
    uint16_t width, uint16_t height);

/* Retrieve only a view baked for exactly this camera/viewport. A stale view
 * is invalidated and returns NOT_FOUND with NULL/0 outputs, rather than
 * drawing old native coordinates after a camera rotation or viewport change.
 * Use begin_camera + append + sort to rebuild it. A legacy begin on a bound
 * slot deliberately invalidates its camera snapshot. */
sat_result_t sat_view_cache_view_camera(
    sat_view_cache_t* cache, uint16_t view,
    const sat_camera3d_t* camera, sat_fx16_t near_depth,
    uint16_t width, uint16_t height,
    const sat_view_cache_item_t** out_items, uint16_t* out_count);

sat_result_t sat_view_cache_set_generation(sat_view_cache_t* cache, uint32_t generation);
sat_result_t sat_view_cache_begin(sat_view_cache_t* cache, uint16_t view);
sat_result_t sat_view_cache_append(sat_view_cache_t* cache,
    const sat_quad2_t* quad, uint32_t depth, uint16_t color, uint16_t tag);

/* For a camera-bound bake, projects one fully visible world quad exactly
 * once and stores both native VDP1 corners and its average camera-space W.
 * Unlike append(), this provides a comparable linear depth for the joint
 * scene painter. Near-plane crossings, unsafe off-screen projections and
 * capacity exhaustion are rejected without mutating the cache. The caller
 * owns world geometry only until this call returns; no pointer is retained. */
sat_result_t sat_view_cache_append_world(
    sat_view_cache_t* cache, const sat_quad3_t* world,
    uint16_t color, uint16_t tag);
sat_result_t sat_view_cache_sort(sat_view_cache_t* cache);
sat_result_t sat_view_cache_view(sat_view_cache_t* cache, uint16_t view,
    const sat_view_cache_item_t** out_items, uint16_t* out_count);
sat_result_t sat_view_cache_stats(const sat_view_cache_t* cache,
    sat_view_cache_stats_t* out);

#ifdef __cplusplus
}
#endif

#endif
