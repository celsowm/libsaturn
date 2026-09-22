#include "saturn/scene.h"

extern "C" sat_result_t sat_scene_submit_transform_instance(
    sat_scene_t* scene, const sat_transform3d_world_t* hierarchy,
    uint16_t node_id, const sat_scene3d_instance_t* prototype,
    uint8_t color_calc_slot, sat_projected_vertex_t* screen_scratch,
    sat_vec3_t* world_scratch) {
    if (!scene || !scene->active || !prototype) return SAT_ERR_INVALID_ARG;
    sat_mat4_t transform{};
    SAT_TRY(sat_transform3d_get_world(hierarchy, node_id, &transform));
    sat_scene3d_instance_t bound = *prototype;
    bound.world = &transform;
    return sat_scene_submit_instance(
        scene, &bound, color_calc_slot, screen_scratch, world_scratch);
}
