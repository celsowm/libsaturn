#ifndef SATURN_SCENE3D_FACES_H
#define SATURN_SCENE3D_FACES_H

#include <stdint.h>
#include "saturn/scene3d.h"

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
    SAT_SCENE3D_INDEXED_TEXTURED = 2,
    SAT_SCENE3D_INDEXED_TILED = 3
} sat_scene3d_material_kind_t;

typedef struct sat_scene3d_material {
    sat_scene3d_material_kind_t kind;
    uint16_t rgb555;
    const sat_vdp1_texture_t* texture;
    /* Only used for INDEXED_TILED; descriptor must survive until flush. */
    const sat_indexed_tiled_quad3_t* tiled;
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

/* Begin using the canonical camera; computes the normalized view forward
 * once. The game no longer needs a second object-painter queue to obtain
 * camera depth and should never calculate painter depth itself. */
sat_result_t sat_scene3d_faces_begin_camera(
    sat_scene3d_faces_t* scene,const sat_camera3d_t* camera,
    sat_fx16_t near_depth,uint16_t width,uint16_t height);

sat_result_t sat_scene3d_faces_depth(
    const sat_scene3d_faces_t* scene,const sat_vec3_t* world,
    sat_fx16_t* out_depth);

/* Material is copied. Both methods are atomic on invalid input/capacity.
 * pass is an explicit artistic override, NOT an occlusion group: all objects
 * needing physical inter-occlusion must use the same pass. */
sat_result_t sat_scene3d_faces_submit_quad(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_scene3d_material_t* material, uint16_t pass);

/* Reuses the renderer's box-face generator, not application-authored winding.
 * Each wall/top enters the global painter independently. */
sat_result_t sat_scene3d_faces_submit_box(
    sat_scene3d_faces_t* scene, const sat_indexed_box3_t* box,
    uint8_t color_calc_slot, uint16_t pass);

/* A textured deck inset with pre-uploaded quadrant fallback. */
sat_result_t sat_scene3d_faces_submit_tiled_quad(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_indexed_tiled_quad3_t* regions,
    uint8_t color_calc_slot, uint16_t pass);

/* Shared mesh instance: world==NULL means an already-prepared world pose
 * (e.g. animated pig); a non-null full world matrix instantiates an immutable
 * LOCAL mesh (e.g. gems). Material bindings and culling belong to the instance,
 * while the scene owns camera projection, face ordering and command emission. */
typedef struct sat_scene3d_instance {
    const sat_mesh_t* mesh;
    const sat_scene3d_material_t* materials;
    uint16_t material_count;
    const uint16_t* face_materials;
    const sat_mat4_t* world; /* NULL: mesh vertices already in world space */
    uint16_t pass;
    uint8_t cull_backfaces;
} sat_scene3d_instance_t;

/* Projects each world vertex once; copies accepted faces into the queue and
 * retains no pointers to the descriptor, matrix or temporary vertex buffers.
 * screen_scratch holds vertex_count projected points. If world != NULL,
 * world_scratch must hold vertex_count transformed points. */
sat_result_t sat_scene3d_faces_submit_instance(
    sat_scene3d_faces_t* scene, const sat_scene3d_instance_t* instance,
    sat_projected_vertex_t* screen_scratch, sat_vec3_t* world_scratch);

/* Emits far-to-near, closes the frame even when hardware submission fails.
 * Caller manages sat_begin_frame/sat_end_frame and HUD command reservation. */
sat_result_t sat_scene3d_faces_flush(sat_scene3d_faces_t* scene);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SCENE3D_FACES_H */
