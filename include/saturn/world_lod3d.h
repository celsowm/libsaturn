#ifndef SATURN_WORLD_LOD3D_H
#define SATURN_WORLD_LOD3D_H

#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_WORLD_LOD3D_API_VERSION 1u
#define SAT_WORLD_LOD3D_MAX_LEVELS 4u
#define SAT_WORLD_LOD3D_MAX_PLANES 6u
#define SAT_WORLD_LOD3D_NO_LOD 0xFFu

/* Bounds and plane offsets are camera-relative 16.16 values. Keeping every
 * bound within +/-16384 units makes all documented 64-bit dot products safe. */
#define SAT_WORLD_LOD3D_COORD_LIMIT_FX 0x40000000

enum {
    SAT_WORLD_LOD3D_OBJECT_OPTIONAL = 1u << 0,
    SAT_WORLD_LOD3D_OBJECT_PINNED = 1u << 1,
    SAT_WORLD_LOD3D_OBJECT_ALLOW_EMPTY_FAR = 1u << 2
};

enum {
    SAT_WORLD_LOD3D_LEVEL_EMPTY = 1u << 0
};

enum {
    SAT_WORLD_LOD3D_DRAW_RESIDENCY_FALLBACK = 1u << 0,
    SAT_WORLD_LOD3D_DRAW_BUDGET_DEGRADED = 1u << 1
};

typedef struct sat_world_lod3d_plane {
    int32_t normal_x, normal_y, normal_z; /* fixed-point unit normal */
    int32_t offset;                       /* inside when dot(n, p) + offset >= 0 */
} sat_world_lod3d_plane_t;

typedef struct sat_world_lod3d_view {
    uint32_t api_version;
    uint32_t struct_size;
    uint32_t generation; /* increment after teleport/scene reset, not origin rebasing */
    int32_t forward_x, forward_y, forward_z; /* normalized 16.16 camera forward */
    uint32_t focal_length_px_16_16;
    uint32_t near_depth_16_16;
    uint16_t viewport_width, viewport_height;
    uint8_t plane_count; /* zero disables frustum culling */
    uint8_t reserved[3];
    sat_world_lod3d_plane_t planes[SAT_WORLD_LOD3D_MAX_PLANES];
} sat_world_lod3d_view_t;

typedef struct sat_world_lod3d_level {
    uint32_t asset_id; /* caller's stable token; the planner never dereferences it */
    uint32_t min_projected_radius_px_16_16;
    uint32_t faces;
    /* Caller estimate; include clipping/fallback allowance. */
    uint32_t command_estimate;
    uint32_t texture_bytes;    /* resident footprint charged to this plan */
    uint32_t slot_generation;
    uint32_t resident_generation;
    uint8_t resident;
    uint8_t flags;
    uint16_t reserved;
} sat_world_lod3d_level_t;

typedef struct sat_world_lod3d_object {
    uint32_t stable_id; /* unique within the input frame */
    uint32_t generation; /* increment when the logical object/asset changes */
    int32_t bounds_min[3]; /* camera-relative 16.16 AABB */
    int32_t bounds_max[3];
    uint32_t levels_first;
    uint32_t lod_hysteresis_px_16_16;
    uint32_t empty_max_projected_radius_px_16_16;
    uint16_t level_count;
    uint16_t flags;
    uint8_t priority; /* 0 yields first; 255 is most important */
    uint8_t reserved[3];
} sat_world_lod3d_object_t;

typedef struct sat_world_lod3d_state {
    uint32_t object_generation;
    uint32_t view_generation;
    uint8_t selected_lod;
    uint8_t valid;
    uint16_t reserved;
} sat_world_lod3d_state_t;

typedef struct sat_world_lod3d_budget {
    uint32_t max_faces;
    uint32_t max_commands;
    uint32_t max_texture_bytes;
    uint8_t max_budget_lod_loss; /* extra coarsening steps after residency fallback */
    uint8_t reserved[3];
} sat_world_lod3d_budget_t;

typedef struct sat_world_lod3d_draw {
    uint32_t stable_id;
    uint32_t object_generation;
    uint32_t asset_id;
    uint32_t residency_generation;
    uint32_t projected_radius_px_16_16;
    int32_t camera_depth_16_16;
    uint32_t faces;
    uint32_t command_estimate;
    uint32_t texture_bytes;
    uint8_t desired_lod; /* projected-size target before hysteresis */
    uint8_t lod;         /* final resident level after hysteresis/budgets */
    uint8_t flags;
    uint8_t reserved;
} sat_world_lod3d_draw_t;

typedef struct sat_world_lod3d_request {
    uint32_t stable_id;
    uint32_t object_generation;
    uint32_t asset_id;
    uint32_t projected_radius_px_16_16;
    uint8_t lod;
    uint8_t priority;
    uint16_t reserved;
} sat_world_lod3d_request_t;

typedef struct sat_world_lod3d_stats {
    uint16_t considered;
    uint16_t frustum_rejected;
    uint16_t no_resident_lod;
    uint16_t stale_residency_objects;
    uint16_t residency_fallbacks;
    uint16_t budget_degraded_objects;
    uint16_t optional_dropped;
    uint16_t empty_selected;
    uint16_t pinned_selected;
    uint16_t draw_count;
    uint16_t prefetch_requests;
    uint16_t desired_by_lod[SAT_WORLD_LOD3D_MAX_LEVELS];
    uint16_t selected_by_lod[SAT_WORLD_LOD3D_MAX_LEVELS];
    uint32_t faces;
    uint32_t commands;
    uint32_t texture_bytes;
} sat_world_lod3d_stats_t;

/* Caller-owned temporary storage, one entry per input object. Do not inspect
 * or modify this storage while a plan call is in progress. */
typedef struct sat_world_lod3d_work {
    uint32_t stable_id;
    uint32_t projected_radius_px_16_16;
    int32_t camera_depth_16_16;
    uint16_t object_index;
    uint8_t desired_lod;
    uint8_t selected_lod;
    uint8_t work_flags;
    uint8_t reserved[3];
} sat_world_lod3d_work_t;

/* Pure CPU planning only: no allocation, IO, VDP1/VDP2 calls, texture uploads,
 * or scene submissions. Objects and LOD tables are immutable. `states[i]`
 * belongs to `objects[i]` and must follow it when the input is reordered.
 * `states`, `draws`, `requests`, `scratch`, and `out_stats` are caller-owned,
 * mutually non-overlapping arrays. Missing
 * desired resources produce explicit prefetch requests; the planner never
 * performs IO. On any error, state, outputs, and statistics remain unchanged.
 * Output is ordered by lowest priority/smallest projected footprint first,
 * with stable_id as the final deterministic tie-break. sat_scene_t remains the
 * only painter/sorter.
 *
 * Level entries are ordered finest to coarsest with strictly descending
 * projected-radius thresholds; the final threshold must be zero. A level is
 * resident only when resident != 0 and slot_generation == resident_generation.
 * Empty levels are selectable only on OPTIONAL objects that also set
 * ALLOW_EMPTY_FAR and only below empty_max_projected_radius_px_16_16.
 * `max_texture_bytes` limits the sum of selected per-object footprints; shared
 * resources should be charged once by the caller or assigned zero per object.
 *
 * The camera forward vector must be normalized within 1% and the focal length
 * must be a positive 16.16 pixel value no greater than 8192 pixels. `generation`
 * is caller-incremented to reset LOD hysteresis after a teleport or scene reset.
 * Keep it unchanged during camera-origin rebasing when camera-relative bounds
 * remain continuous; rebasing alone must not disturb the selected level. */
sat_result_t sat_world_lod3d_plan(
    const sat_world_lod3d_view_t* view,
    const sat_world_lod3d_budget_t* budget,
    const sat_world_lod3d_object_t* objects, uint16_t object_count,
    const sat_world_lod3d_level_t* levels, uint16_t level_count,
    sat_world_lod3d_state_t* states,
    sat_world_lod3d_draw_t* draws, uint16_t draw_capacity,
    sat_world_lod3d_request_t* requests, uint16_t request_capacity,
    sat_world_lod3d_work_t* scratch, uint16_t scratch_capacity,
    sat_world_lod3d_stats_t* out_stats);

#ifdef __cplusplus
}
#endif

#endif
