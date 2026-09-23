#ifndef SATURN_TEXTURE_H
#define SATURN_TEXTURE_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/surface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Logical game-facing texture handle. Hardware addresses and palette banks are
 * intentionally absent; the runtime owns those details. */
typedef struct sat_texture {
    uint16_t slot;
    uint16_t generation;
} sat_texture_t;

typedef enum sat_texture_backing_policy {
    /* Upload pixels once and retain no CPU-side source. Region preparation is
     * unavailable after creation. */
    SAT_TEXTURE_UPLOAD_ONLY = 0,
    /* Retain a non-owning reference to caller storage for region preparation.
     * The caller must keep pixels and palette alive for the texture lifetime. */
    SAT_TEXTURE_PERSISTENT_SOURCE = 1,
    /* As PERSISTENT_SOURCE, but sat_texture_update_rect may also write updates
     * back into the caller-owned source buffer. */
    SAT_TEXTURE_DYNAMIC = 2
} sat_texture_backing_policy_t;

/* An in-place VRAM/CRAM update cannot be rolled back after a hardware error.
 * Dirty textures remain destroyable and may be repaired by a full update.
 * Neither a dirty parent nor its previously prepared regions may be drawn. */
typedef enum sat_texture_health {
    SAT_TEXTURE_READY = 0,
    SAT_TEXTURE_NEEDS_RECOVERY = 1
} sat_texture_health_t;

typedef struct sat_texture_info {
    uint16_t width;
    uint16_t height;
    sat_pixel_format_t format;
    sat_texture_backing_policy_t backing_policy;
    uint16_t prepared_region_count;
    sat_texture_health_t health;
} sat_texture_info_t;

typedef struct sat_texture_region_stats {
    uint16_t used;
    uint16_t capacity;
} sat_texture_region_stats_t;

/* Current first implementation accepts INDEX8 surfaces with exactly 256
 * RGB555 palette entries and VDP1-legal dimensions (width multiple of 8,
 * 8..504; height 1..255). Other source formats return SAT_ERR_UNSUPPORTED
 * rather than being silently reinterpreted. */
sat_result_t sat_texture_create_from_surface(
    sat_texture_t* out_texture,
    const sat_surface_t* source,
    sat_texture_backing_policy_t backing_policy
);

sat_result_t sat_texture_destroy(sat_texture_t texture);
sat_result_t sat_texture_info(sat_texture_t texture, sat_texture_info_t* out_info);

/* Replaces the complete texture contents. Width, height and format must match
 * the original texture. PERSISTENT_SOURCE and DYNAMIC retain the non-owning
 * new source descriptor and refresh prepared regions in place. Hardware
 * failures can partially change CRAM/VRAM: the handle remains alive but is
 * marked NEEDS_RECOVERY, and draws/region preparation are rejected. Repeating
 * this full update with a valid source repairs the parent and all regions.
 * The caller must keep the new source and palette alive until the update
 * has completed. Palette ownership is committed after its successful upload,
 * but a failed in-place update is not an atomic hardware rollback. */
sat_result_t sat_texture_update(sat_texture_t texture, const sat_surface_t* source);

/* Writes a rectangular update. This is limited to DYNAMIC textures because
 * the caller-owned retained source is modified in place before the VRAM
 * refresh. On a hardware failure that CPU source may already contain the
 * patch; the texture is marked NEEDS_RECOVERY. Call sat_texture_update with
 * the full retained source to repair it. A dirty texture rejects partial
 * updates until repaired. source must match destination_rect and palette. */
sat_result_t sat_texture_update_rect(
    sat_texture_t texture,
    const sat_rect_t* destination_rect,
    const sat_surface_t* source
);

/* Prepares a source rectangle for future sprite-sheet drawing. The current
 * VDP1 cache supports arbitrary X/Y and height but requires region width to be
 * a multiple of 8, matching the VDP1 character-pattern width rule. The call is
 * idempotent. UPLOAD_ONLY textures return SAT_ERR_UNSUPPORTED. */
sat_result_t sat_texture_prepare_region(sat_texture_t texture, const sat_rect_t* region);

sat_result_t sat_texture_region_stats(
    sat_texture_t texture,
    sat_texture_region_stats_t* out_stats
);

/* Fixed deterministic capacities; useful to size content budgets and tests. */
uint16_t sat_texture_capacity(void);
uint16_t sat_texture_region_capacity(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_TEXTURE_H */
