#ifndef SATURN_SCENE_FRAME_RESULT_HPP
#define SATURN_SCENE_FRAME_RESULT_HPP

#include "saturn/scene.h"

namespace saturn::core::scene {

// Only L3 scene operations record errors. SAT_ERR_BUSY from a valid pending
// task is transient and must not be passed to this helper.
inline sat_result_t record_frame_result(sat_scene_t* scene, sat_result_t status) {
    if (scene != nullptr && scene->active != 0u &&
        status != SAT_OK && scene->first_error == SAT_OK)
        scene->first_error = status;
    return status;
}

} // namespace saturn::core::scene

#endif
