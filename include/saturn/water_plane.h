#ifndef SATURN_WATER_PLANE_H
#define SATURN_WATER_PLANE_H

#include "saturn/mesh3d.h"
#include "saturn/render3d.h"
#include "saturn/scene.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A large horizontal water surface as camera-centred rings of quads, fine
 * near the camera and twice as coarse with each ring outward. A level models
 * its lake as a few huge quads; the near plane cuts them, and a cut face
 * leaves the projected fast path (a textured one is dropped). The rings keep
 * every face near the camera small, share vertices inside a ring so each is
 * projected once, and leave a hole in each outer ring where the inner ring
 * already covers the water.
 *
 * Ring k has cells x cells quads of size cell * 2^k, centred on the camera
 * position snapped to twice that size, so vertices stay put while the camera
 * moves. Cells wholly inside the next-finer ring are left out. Vertices are
 * clamped to the region rectangle, and cells that collapse are dropped. */
#define SAT_WATER_PLANE_RINGS_MAX 4u
#define SAT_WATER_PLANE_CELLS_MAX 8u
#define SAT_WATER_PLANE_VERTICES ((SAT_WATER_PLANE_CELLS_MAX + 1u) * (SAT_WATER_PLANE_CELLS_MAX + 1u))
#define SAT_WATER_PLANE_FACES (SAT_WATER_PLANE_CELLS_MAX * SAT_WATER_PLANE_CELLS_MAX)

typedef struct sat_water_plane_config {
    sat_fx16_t min_x, max_x, min_z, max_z; /* region rectangle, min < max */
    sat_fx16_t surface;
    sat_fx16_t cell;     /* finest cell size, > 0 */
    uint8_t cells;       /* cells per ring side: 4 or 8 */
    uint8_t rings;       /* 1..SAT_WATER_PLANE_RINGS_MAX */
    uint8_t underside;   /* nonzero: wound to face down, for a camera below */
    /* Optional sine wobble of the finest ring, fading to flat at its edge so
     * it meets the flat outer rings without a crack. wobble 0 is flat. */
    sat_fx16_t wobble;   /* peak height */
    sat_fx16_t period;   /* world length of one wave; > 0 when wobble != 0 */
    int32_t speed;       /* degrees of phase per tick */
    uint32_t tick;
    /* Gouraud entries (sat_gouraud_rgb) blended by distance from the camera,
     * from the finest ring's centre to the outer edge of the last ring. */
    uint16_t near_color, far_color;
    int32_t glint;       /* levels added at a wave crest (and removed in a trough) */
    /* Optional cull of cells behind the camera: forward is the horizontal
     * view direction, behind the margin in world units a cell is dropped.
     * A zero forward vector disables it. Use a margin of at least the
     * camera height above the surface when the camera can look down. */
    sat_fx16_t forward_x, forward_z, behind;
} sat_water_plane_config_t;

typedef struct sat_water_plane_ring {
    sat_mesh_t mesh;
    uint16_t light[SAT_WATER_PLANE_VERTICES];
    sat_vec3_t vertices[SAT_WATER_PLANE_VERTICES];
    uint16_t indices[4u * SAT_WATER_PLANE_FACES];
} sat_water_plane_ring_t;

/* Caller-owned; build refills it every frame. */
typedef struct sat_water_plane {
    sat_water_plane_ring_t ring[SAT_WATER_PLANE_RINGS_MAX];
    uint8_t ring_count;
    uint16_t face_materials[SAT_WATER_PLANE_FACES]; /* all zero */
    sat_projected_vertex_t scratch[SAT_WATER_PLANE_VERTICES];
} sat_water_plane_t;

/* Fills the rings for a camera at (camera_x, camera_z). The faces of every
 * ring together never exceed rings * cells * cells. */
sat_result_t sat_water_plane_build(sat_water_plane_t* plane,
    const sat_water_plane_config_t* config, sat_fx16_t camera_x, sat_fx16_t camera_z);

/* Total faces the last build produced. */
sat_result_t sat_water_plane_face_count(const sat_water_plane_t* plane, uint16_t* out);

/* Submits every non-empty ring to the scene at `pass`, ring k with
 * materials[k] (so a coarser ring can use a texture baked at a lower
 * resolution). The material's vertex_gouraud is replaced by the ring's own
 * light. Back faces are culled, so a one-sided plane needs `underside` set
 * when the camera is below it. */
sat_result_t sat_water_plane_submit(sat_scene_t* scene, sat_water_plane_t* plane,
    const sat_scene3d_material_t* materials, uint16_t pass);

#ifdef __cplusplus
}
#endif
#endif
