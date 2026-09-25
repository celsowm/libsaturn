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

} // namespace saturn::graphics::scene3d

#endif
