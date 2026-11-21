#ifndef SATURN_WATER_ACTOR_H
#define SATURN_WATER_ACTOR_H

#include "saturn/mesh3d.h"
#include "saturn/render3d.h"
#include "saturn/scene.h"
#include "saturn/water.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A moving mesh (a character, a ship, a ball) crossing one water surface.
 * The mesh is in world space, as after sat_skin_update_mesh plus the actor's
 * position. Faces wholly above or below the surface go out as two instances,
 * the below ones tinted by depth and drawn in `pass_below`, before the water;
 * only the faces that cross the surface are cut, with sat_water_split_quad_shaded.
 * A textured face cannot be cut, so it joins whichever side holds its middle.
 * Cut RGB pieces are drawn without back-face culling, since the scene's quad
 * path has none; they are few. The instances keep the caller's cull_backfaces.
 *
 * Two steps: sat_water_actor_split fills caller storage and is pure, and
 * sat_water_actor_submit sends the result to the scene. */
#define SAT_WATER_ACTOR_MATERIALS_MAX 16u

typedef struct sat_water_actor_workspace {
    uint16_t* above_indices;     /* 4 * face_cap */
    uint16_t* above_materials;   /* face_cap */
    uint16_t* below_indices;     /* 4 * face_cap */
    uint16_t* below_materials;   /* face_cap */
    uint16_t* wet_light;         /* vertex_cap: base light tinted by depth */
    sat_water_shaded_piece_t* pieces;  /* piece_cap */
    uint16_t* piece_materials;   /* piece_cap */
    sat_projected_vertex_t* scratch;   /* vertex_cap, used by submit */
    uint16_t face_cap, vertex_cap, piece_cap;
} sat_water_actor_workspace_t;

typedef struct sat_water_actor_params {
    sat_fx16_t surface;
    int16_t tint[3];          /* gouraud levels at full depth: R, G, B */
    sat_fx16_t full_depth;    /* > 0 */
    uint16_t pass_below, pass_above;
} sat_water_actor_params_t;

typedef struct sat_water_actor_split {
    uint16_t above_faces, below_faces, pieces;
    uint16_t uncut_faces;     /* crossing textured faces left whole */
} sat_water_actor_split_t;

/* `base_light` (optional, per vertex) is the mesh's own Gouraud light; the
 * wet table adds the tint to it. SAT_ERR_CAPACITY if the workspace is too
 * small for this mesh; the result is then unspecified. */
sat_result_t sat_water_actor_split(const sat_mesh_t* mesh,
    const uint16_t* face_materials, const sat_scene3d_material_t* materials,
    uint16_t material_count, const uint16_t* base_light,
    const sat_water_actor_params_t* params, sat_water_actor_workspace_t* workspace,
    sat_water_actor_split_t* out);

/* Submits what the last split produced. `materials` is the same array given to
 * the split; RGB and LUT4 materials get the dry or wet light table, other
 * kinds are submitted as they are. A mesh with no faces on a side submits
 * nothing for it. */
sat_result_t sat_water_actor_submit(sat_scene_t* scene, const sat_mesh_t* mesh,
    const sat_scene3d_material_t* materials, uint16_t material_count,
    const uint16_t* base_light, uint8_t cull_backfaces,
    const sat_water_actor_params_t* params, sat_water_actor_workspace_t* workspace,
    const sat_water_actor_split_t* split);

#ifdef __cplusplus
}
#endif
#endif
