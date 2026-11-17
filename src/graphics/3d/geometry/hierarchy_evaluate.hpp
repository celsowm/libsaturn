#pragma once
#include "saturn/transform3d.h"

// Shared traversal for the production comparator and experimental kernels.
// Preserve validation, arbitrary parent order, roots and dirty propagation.
namespace saturn::core::hierarchy {
template<class Compose>
inline sat_result_t evaluate(
    sat_transform3d_world_t* world, uint16_t* scratch,
    uint16_t scratch_capacity, Compose compose) {
    if (!world || !world->nodes || !world->capacity ||
        world->count > world->capacity || !scratch)
        return SAT_ERR_INVALID_ARG;
    if (scratch_capacity < world->count) return SAT_ERR_CAPACITY;
    if (!world->pending) return SAT_OK;

    /* 0 unvisited, 1 evaluated/unchanged, 2 evaluated/changed, 3 visiting.
     * Each node is pushed/popped once, regardless of creation or parent order. */
    for (uint16_t i = 0; i < world->count; ++i)
        world->nodes[i].eval_state = 0;
    for (uint16_t id = 0; id < world->count; ++id) {
        if (world->nodes[id].eval_state) continue;
        uint16_t length = 0;
        uint16_t current = id;
        while (current != SAT_TRANSFORM3D_ROOT &&
               world->nodes[current].eval_state == 0) {
            if (length >= world->count) return SAT_ERR_INVALID_ARG;
            scratch[length++] = current;
            world->nodes[current].eval_state = 3;
            const uint16_t parent = world->nodes[current].parent;
            if (parent != SAT_TRANSFORM3D_ROOT && parent >= world->count)
                return SAT_ERR_INVALID_ARG;
            current = parent;
        }
        if (current != SAT_TRANSFORM3D_ROOT &&
            world->nodes[current].eval_state == 3)
            return SAT_ERR_INVALID_ARG;
        while (length) {
            sat_transform3d_node_t& node = world->nodes[scratch[--length]];
            const bool parent_changed =
                node.parent != SAT_TRANSFORM3D_ROOT &&
                world->nodes[node.parent].eval_state == 2;
            const bool changed = node.dirty || parent_changed;
            if (changed) {
                sat_mat4_t local_matrix{};
                if(node.use_local_matrix)local_matrix=node.local_matrix;
                else SAT_TRY(sat_model_transform3d_matrix(&node.local, &local_matrix));
                if (node.parent == SAT_TRANSFORM3D_ROOT) {
                    node.world = local_matrix;
                } else {
                    sat_mat4_t composite{};
                    SAT_TRY(compose(
                        &composite, &world->nodes[node.parent].world,
                        &local_matrix));
                    node.world = composite;
                }
                node.dirty = 0;
            }
            node.eval_state = changed ? 2 : 1;
        }
    }
    world->pending = 0;
    return SAT_OK;
}
}
