#ifndef SATURN_SCENE3D_MATERIAL_POOL_H
#define SATURN_SCENE3D_MATERIAL_POOL_H

#include <stdint.h>
#include "saturn/scene3d_faces.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Startup-only, caller-owned pool for VDP1 solid indexed sprites. Deduplicates
 * identical RGB555 colours across world, props and animated models. Every
 * unique colour receives ONE 8x8 INDEX8 texture and ONE palette entry; a
 * repeated colour returns its original handle without VRAM/CRAM upload.
 * The application selects the CRAM bank and owns all memory and slot policy.
 * 255 colours max because INDEX8 palette index zero is transparent. */
typedef struct sat_scene3d_solid_pool {
    sat_scene3d_material_t* materials;
    sat_vdp1_texture_t* textures;
    uint16_t* colors;
    uint8_t* pixels; /* caller-owned 8x8 temporary upload buffer */
    uint16_t count, capacity, palette_bank;
} sat_scene3d_solid_pool_t;

sat_result_t sat_scene3d_solid_pool_init(
    sat_scene3d_solid_pool_t* pool,
    sat_scene3d_material_t* material_storage,
    sat_vdp1_texture_t* texture_storage,
    uint16_t* color_storage, uint8_t pixels[64],
    uint16_t capacity, uint16_t palette_bank);

sat_result_t sat_scene3d_solid_pool_register(
    sat_scene3d_solid_pool_t* pool, uint16_t rgb555,
    uint16_t* out_material_index);

/* Nearest registered colour by squared RGB555 distance, returning early on an
 * exact match. A palette chosen for fade or theming rarely contains the exact
 * colour a piece of level code asks for, and the pool already holds the
 * registered colours, so neither the search nor a parallel colour table
 * belongs in application code. SAT_ERR_NOT_FOUND while the pool is empty. */
sat_result_t sat_scene3d_solid_pool_find_nearest(
    const sat_scene3d_solid_pool_t* pool, uint16_t rgb555,
    uint16_t* out_material_index);

/* The same search restricted to entries [first, first+count). One pool often
 * holds several palettes -- a hand-authored world palette and an imported
 * model's shade ramp, say -- and matching a world colour against the model's
 * shades would silently pick a material the level never meant to use. The
 * range is validated against the pool, so a stale count is rejected instead of
 * reading past the registered colours. */
sat_result_t sat_scene3d_solid_pool_find_nearest_in(
    const sat_scene3d_solid_pool_t* pool, uint16_t rgb555,
    uint16_t first, uint16_t count, uint16_t* out_material_index);

/* Call after registering all startup colours. Repeated palette uploads are
 * not required for individual instances; the pool does not own VDP2 fade
 * slot state or modify it. Hardware failures may consume VRAM during a
 * partially completed startup; abort initialization on upload failure. */
sat_result_t sat_scene3d_solid_pool_upload_palette(
    const sat_scene3d_solid_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif
