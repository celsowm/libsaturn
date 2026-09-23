#include "saturn/scene.h"
#include "src/graphics/3d/scene/frame_result.hpp"

extern "C" sat_result_t sat_scene_submit_transform_instance(
    sat_scene_t* scene, const sat_transform3d_world_t* hierarchy,
    uint16_t node_id, const sat_scene3d_instance_t* prototype,
    uint8_t color_calc_slot, sat_projected_vertex_t* screen_scratch,
    sat_vec3_t* world_scratch) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    if (!prototype)
        return saturn::core::scene::record_frame_result(scene,SAT_ERR_INVALID_ARG);
    sat_mat4_t transform{};
    const sat_result_t resolved=sat_transform3d_get_world(
        hierarchy, node_id, &transform);
    if (resolved!=SAT_OK)
        return saturn::core::scene::record_frame_result(scene,resolved);
    sat_scene3d_instance_t bound = *prototype;
    bound.world = &transform;
    return sat_scene_submit_instance(
        scene, &bound, color_calc_slot, screen_scratch, world_scratch);
}
