#ifndef SATURN_WATER_PREPARE_H
#define SATURN_WATER_PREPARE_H

#include "saturn/mesh3d.h"
#include "saturn/scene3d_faces.h"
#include "saturn/water.h"
#include "saturn/water_regions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One-shot preparation of a static level mesh against its water regions, so
 * the per-frame cost of "what is under the water" is nil. Run it at load, in
 * a host tool, or again when a region's surface moves; it is not meant to
 * run every frame.
 *
 * Every face is classified by the region over its centroid:
 *   above    entirely at or over that region's surface (or in no region);
 *   below    entirely under it;
 *   crossing cut at the surface into above and below pieces, when its
 *            material is a plain RGB one. A textured face cannot be cut (VDP1
 *            maps the whole texture onto a quad), so it joins below when its
 *            centroid is under the surface and above otherwise, and is
 *            counted in uncut_faces.
 * The output lists the above faces first and the below faces after them, over
 * one vertex array, so a game submits two instances: the below faces in a
 * pass before the water and the above faces in a pass after it.
 *
 * Each vertex also gets a Gouraud entry: the input's own entry (or neutral)
 * shifted by `tint` in proportion to its depth under its region's surface,
 * reaching the full tint at full_depth. Set a material's vertex_gouraud to
 * `gouraud` for RGB and LUT4 materials; INDEX8 does not take Gouraud. */
typedef struct sat_water_prepare_config {
    const sat_water_regions_t* regions;
    int16_t tint[3];          /* levels at full depth: R, G, B (negative darkens) */
    sat_fx16_t full_depth;    /* > 0 */
} sat_water_prepare_config_t;

/* Caller-owned output storage (the *_cap fields) and the result counts. */
typedef struct sat_water_prepared {
    sat_vec3_t* vertices;       /* vertex_cap entries */
    uint16_t* gouraud;          /* vertex_cap entries */
    uint16_t* indices;          /* 4 * face_cap entries */
    uint16_t* face_materials;   /* face_cap entries */
    uint16_t vertex_cap, face_cap;
    uint16_t vertex_count;
    uint16_t above_faces, below_faces;
    uint16_t cut_faces;         /* faces that were split at a surface */
    uint16_t uncut_faces;       /* crossing faces left whole (not cuttable) */
} sat_water_prepared_t;

/* `base_gouraud` (optional, one entry per input vertex) and `face_materials`
 * (optional, defaults to material 0) describe the input. Fails with
 * SAT_ERR_CAPACITY when the output storage is too small; the result is then
 * unspecified. */
sat_result_t sat_water_prepare_mesh(const sat_mesh_t* mesh,
    const uint16_t* face_materials, const sat_scene3d_material_t* materials,
    uint16_t material_count, const uint16_t* base_gouraud,
    const sat_water_prepare_config_t* config, sat_water_prepared_t* io);

#ifdef __cplusplus
}
#endif
#endif
