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

typedef struct sat_texture_info {
    uint16_t width;
    uint16_t height;
    sat_pixel_format_t format;
    sat_texture_backing_policy_t backing_policy;
    uint16_t prepared_region_count;
    uint16_t reserved;
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
 * the original texture. PERSISTENT_SOURCE and DYNAMIC textures retain the new
 * source descriptor; prepared regions are invalidated. */
sat_result_t sat_texture_update(sat_texture_t texture, const sat_surface_t* source);

/* Writes a rectangular update. This is intentionally limited to DYNAMIC
 * textures because the runtime must keep its retained source coherent for
 * later region preparation. source must have exactly destination_rect size and
 * the same format/palette semantics as the texture. */
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
