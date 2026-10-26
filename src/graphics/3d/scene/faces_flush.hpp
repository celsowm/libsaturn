#pragma once

#include "saturn/scene3d_faces.h"

namespace saturn::internal {

/* One pass over a projected queue: the commands its faces need at least
 * (every hot face, and each projected-safe cold one), and whether every face
 * may be emitted, as sat_scene3d_faces_flush checks before emitting. */
bool projected_census(const sat_scene3d_faces_t& faces, uint32_t* commands);

/* sat_scene3d_faces_flush; `queue_valid` skips its own pass over a
 * projected queue that projected_census just found valid. */
sat_result_t faces_flush(sat_scene3d_faces_t* scene, bool queue_valid);

}  // namespace saturn::internal
