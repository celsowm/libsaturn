#ifndef SATURN_WORLD_LOD_FADE3D_H
#define SATURN_WORLD_LOD_FADE3D_H

#include <stdint.h>

#include "saturn/fade3d.h"
#include "saturn/scene3d_faces.h"
#include "saturn/world_lod3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sat_world_lod3d_fade_fallback {
    SAT_WORLD_LOD3D_FADE_FALLBACK_OPAQUE = 0,
    SAT_WORLD_LOD3D_FADE_FALLBACK_CULL = 1
} sat_world_lod3d_fade_fallback_t;

typedef enum sat_world_lod3d_fade_disposition {
    SAT_WORLD_LOD3D_FADE_DISABLED = 0,
    SAT_WORLD_LOD3D_FADE_APPLIED = 1,
    SAT_WORLD_LOD3D_FADE_OPAQUE_UNSUPPORTED = 2,
    SAT_WORLD_LOD3D_FADE_CULLED_BY_DISTANCE = 3,
    SAT_WORLD_LOD3D_FADE_CULLED_UNSUPPORTED = 4
} sat_world_lod3d_fade_disposition_t;

/* Describes configuration established by the application before drawing.
 * `normal_sprite_priority` is the value passed to
 * sat_vdp2_sprite_color_calc_configure[_alpha](); the faded selector is one
 * priority lower. `backdrop_priority` is the VDP2 layer the fade must blend
 * with. The helper validates this ordering but does not write registers. */
typedef struct sat_world_lod3d_fade_composition {
    uint8_t fade_enabled;
    uint8_t color_calc_configured;
    uint8_t normal_sprite_priority;
    uint8_t backdrop_priority;
    uint8_t unsupported_fallback;
    uint8_t reserved[3];
} sat_world_lod3d_fade_composition_t;

/* One per logical object. Fade history intentionally survives selected LOD
 * changes; increment object_generation when the logical object or its
 * material family changes, and view_generation after a teleport or scene
 * reset. Camera-origin rebasing alone must not reset it. */
typedef struct sat_world_lod3d_fade_state {
    uint32_t object_generation;
    uint32_t view_generation;
    uint8_t color_calc_slot;
    uint8_t valid;
    uint16_t reserved;
} sat_world_lod3d_fade_state_t;

typedef struct sat_world_lod3d_fade_result {
    uint8_t draw;
    uint8_t color_calc_slot; /* selected material slot; never FADE3D_SLOT_CULLED */
    uint8_t disposition;
    uint8_t reserved;
} sat_world_lod3d_fade_result_t;

/* Applies the existing sat_fade3d_slot() policy to a selected planner draw.
 * Only valid INDEXED8 scene materials may use VDP2 ratio slots. RGB polygons,
 * LUT4/RGB-coded texture output, missing lower-priority backdrops and
 * unconfigured hardware take the explicit opaque/cull fallback. The cull
 * fallback must be authored as acceptable by the caller. This function is
 * pure CPU logic: it never configures VDP2, emits commands or allocates.
 *
 * `draw->camera_depth_16_16` is passed directly to sat_fade3d_slot(). On
 * errors, state and both outputs remain unchanged. `out_material` receives a
 * copy of an otherwise valid scene material suitable for the canonical
 * sat_scene_t painter. `state`, `out_material` and `out_result` must not
 * overlap. The color-calc table and priority selectors must already be
 * configured for the current video state. */
sat_result_t sat_world_lod3d_fade_apply(
    const sat_fade3d_slots_t* fade,
    const sat_world_lod3d_draw_t* draw,
    const sat_scene3d_material_t* material,
    const sat_world_lod3d_fade_composition_t* composition,
    uint32_t view_generation,
    sat_world_lod3d_fade_state_t* state,
    sat_scene3d_material_t* out_material,
    sat_world_lod3d_fade_result_t* out_result);

#ifdef __cplusplus
}
#endif

#endif
