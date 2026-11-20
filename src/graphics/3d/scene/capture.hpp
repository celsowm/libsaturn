#ifndef SATURN_GRAPHICS_SCENE3D_CAPTURE_HPP
#define SATURN_GRAPHICS_SCENE3D_CAPTURE_HPP

#include "saturn/scene3d_faces.h"

namespace saturn::graphics::scene3d {

/* Hook for the direct world-quad draws of render3d.h: while
 * sat_scene3d_capture_begin has a scene open, queues quad with material into
 * it, stores the submission result and returns true; otherwise returns false
 * and the caller draws immediately. Lives with the scene (faces.cpp). */
bool capture_quad(const sat_quad3_t& quad, const sat_scene3d_material_t& material,
                  sat_result_t* result);

/* Flush fallback for a textured face (indexed.cpp): the textured quad3 draw
 * (gouraud may be NULL), then, if it drew nothing and fallback_rgb555 != 0,
 * the near-clipped solid polygon -- sharing one depth test and projection. */
sat_result_t draw_textured_quad_or_solid(
    const sat_quad3_t& quad, const sat_indexed_solid_render3d_t& p,
    const sat_vdp1_texture_t* texture, const uint16_t* gouraud, uint16_t fallback_rgb555);

} // namespace saturn::graphics::scene3d

#endif
