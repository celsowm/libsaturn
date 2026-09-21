#ifndef P3D_GHOST_MODEL_H
#define P3D_GHOST_MODEL_H

/* A ghost: a Gouraud-shaded dome on a flared skirt, with a textured face. */

#include <stdint.h>

#include "saturn/math3d.h"
#include "saturn/scene.h"

#include "camera.h"

typedef enum p3d_ghost_mood {
    P3D_GHOST_HUNTING = 0,   /* eyes, pupils looking where it goes */
    P3D_GHOST_FRIGHTENED,    /* blue, pale face */
    P3D_GHOST_FLASHING       /* white, red face: fright is running out */
} p3d_ghost_mood_t;

/* Builds the mesh and uploads the face textures; needs the VDP1 up. */
void p3d_ghost_init(void);

/* Re-shades for a new key light (world space). Call when the camera turns. */
void p3d_ghost_relight(const sat_vec3_t* light);

void p3d_ghost_draw(sat_scene_t* scene, const p3d_camera_t* camera,
                    int x, int z, int dir, uint16_t color, p3d_ghost_mood_t mood);

#endif /* P3D_GHOST_MODEL_H */
