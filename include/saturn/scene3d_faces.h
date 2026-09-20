#ifndef SATURN_SCENE3D_FACES_H
#define SATURN_SCENE3D_FACES_H

#include <stdint.h>
#include "saturn/mesh3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A bounded scene-wide face painter. Every submitted face, including faces
 * from DIFFERENT meshes, enters the same ordered list. This does not provide
 * a Z-buffer: crossing polygons may need level-authored subdivision.
 * No allocation or implicit texture uploads occur during a frame. */
typedef enum sat_scene3d_material_kind {
    SAT_SCENE3D_RGB = 0,
    SAT_SCENE3D_INDEXED_SOLID = 1,
    SAT_SCENE3D_INDEXED_TEXTURED = 2
} sat_scene3d_material_kind_t;

typedef struct sat_scene3d_material {
    sat_scene3d_material_kind_t kind;
    uint16_t rgb555;
    const sat_vdp1_texture_t* texture;
    /* 255 = ordinary opaque sprite; 0..7 = previously configured VDP2 slot.
     * RGB polygon material does not support the indexed-sprite slots. */
    uint8_t color_calc_slot;
} sat_scene3d_material_t;

typedef struct sat_scene3d_face {
    sat_quad3_t world;
    sat_quad2_t projected;
    sat_scene3d_material_t material;
    int64_t depth;
    uint32_t sequence;
    uint16_t pass;
    uint8_t projected_safe;
} sat_scene3d_face_t;

typedef struct sat_scene3d_faces {
    sat_scene3d_face_t* entries;
    uint16_t capacity;
    uint16_t count;
    uint16_t width, height;
    sat_mat4_t view_proj;
    sat_vec3_t eye, forward;
    sat_fx16_t near_depth;
    uint8_t active;
} sat_scene3d_faces_t;

sat_result_t sat_scene3d_faces_init(
    sat_scene3d_faces_t* scene, sat_scene3d_face_t* storage,
    uint16_t capacity);

sat_result_t sat_scene3d_faces_begin(
    sat_scene3d_faces_t* scene, const sat_mat4_t* view_proj,
    const sat_vec3_t* eye, const sat_vec3_t* forward,
    sat_fx16_t near_depth, uint16_t width, uint16_t height);

/* Material is copied. Both methods are atomic on invalid input/capacity.
 * pass is an explicit artistic override, NOT an occlusion group: all objects
 * needing physical inter-occlusion must use the same pass. */
sat_result_t sat_scene3d_faces_submit_quad(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_scene3d_material_t* material, uint16_t pass);

/* The mesh may already be transformed to world space (translation=NULL),
 * e.g. sat_anim_prepare_model_instance's output. Alternatively, the same
 * immutable LOCAL mesh may be submitted with a different translation.
 * screen_scratch must hold mesh->vertex_count projected vertices; when
 * translation != NULL, world_scratch must also hold that many world vertices.
 * Projection is performed ONCE per vertex, never once per face.
 * Face materials index a caller-owned per-instance material table. */
sat_result_t sat_scene3d_faces_submit_mesh(
    sat_scene3d_faces_t* scene, const sat_mesh_t* mesh,
    const sat_vec3_t* translation,
    const sat_scene3d_material_t* materials, uint16_t material_count,
    const uint16_t* face_materials, uint16_t pass, uint8_t cull_backfaces,
    sat_projected_vertex_t* screen_scratch,
    sat_vec3_t* world_scratch);

/* Emits far-to-near, closes the frame even when hardware submission fails.
 * Caller manages sat_begin_frame/sat_end_frame and HUD command reservation. */
sat_result_t sat_scene3d_faces_flush(sat_scene3d_faces_t* scene);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SCENE3D_FACES_H */
