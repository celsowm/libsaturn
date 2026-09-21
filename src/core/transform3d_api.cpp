#include "saturn/transform3d.h"

namespace {
bool is_live(const sat_transform3d_world_t* world, uint16_t id) {
    return world && world->nodes && world->capacity &&
           world->count <= world->capacity && id < world->count;
}
}

extern "C" sat_result_t sat_transform3d_world_init(
    sat_transform3d_world_t* world, sat_transform3d_node_t* nodes,
    uint16_t capacity) {
    if (!world || !nodes || !capacity || capacity == SAT_TRANSFORM3D_ROOT)
        return SAT_ERR_INVALID_ARG;
    world->nodes = nodes;
    world->capacity = capacity;
    world->count = 0;
    world->pending = 0;
    return SAT_OK;
}

extern "C" void sat_transform3d_world_reset(sat_transform3d_world_t* world) {
    if (!world || !world->nodes) return;
    world->count = 0;
    world->pending = 0;
}

extern "C" sat_result_t sat_transform3d_create(
    sat_transform3d_world_t* world, uint16_t* out_id) {
    if (!world || !world->nodes || !out_id || !world->capacity ||
        world->count > world->capacity) return SAT_ERR_INVALID_ARG;
    if (world->count == world->capacity) return SAT_ERR_CAPACITY;
    const uint16_t id = world->count;
    sat_transform3d_node_t& node = world->nodes[id];
    sat_model_transform3d_identity(&node.local);
    SAT_TRY(sat_mat4_identity(&node.world));
    node.parent = SAT_TRANSFORM3D_ROOT;
    node.dirty = 1;
    node.eval_state = 0;
    world->count = (uint16_t)(id + 1u);
    world->pending = 1;
    *out_id = id;
    return SAT_OK;
}

extern "C" sat_result_t sat_transform3d_set_parent(
    sat_transform3d_world_t* world, uint16_t id, uint16_t parent) {
    if (!is_live(world, id) ||
        (parent != SAT_TRANSFORM3D_ROOT && !is_live(world, parent)))
        return SAT_ERR_INVALID_ARG;
    if (id == parent) return SAT_ERR_INVALID_ARG;
    uint16_t ancestor = parent;
    for (uint16_t steps = 0; ancestor != SAT_TRANSFORM3D_ROOT; ++steps) {
        if (steps >= world->count || ancestor == id)
            return SAT_ERR_INVALID_ARG;
        ancestor = world->nodes[ancestor].parent;
        if (ancestor != SAT_TRANSFORM3D_ROOT && ancestor >= world->count)
            return SAT_ERR_INVALID_ARG;
    }
    sat_transform3d_node_t& node = world->nodes[id];
    if (node.parent != parent) {
        node.parent = parent;
        node.dirty = 1;
        world->pending = 1;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_transform3d_set_local(
    sat_transform3d_world_t* world, uint16_t id,
    const sat_model_transform3d_t* local) {
    if (!is_live(world, id) || !local) return SAT_ERR_INVALID_ARG;
    world->nodes[id].local = *local;
    world->nodes[id].dirty = 1;
    world->pending = 1;
    return SAT_OK;
}

extern "C" sat_result_t sat_transform3d_evaluate(
    sat_transform3d_world_t* world, uint16_t* scratch,
    uint16_t scratch_capacity) {
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
                SAT_TRY(sat_model_transform3d_matrix(&node.local, &local_matrix));
                if (node.parent == SAT_TRANSFORM3D_ROOT) {
                    node.world = local_matrix;
                } else {
                    sat_mat4_t composite{};
                    SAT_TRY(sat_mat4_multiply(
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

extern "C" sat_result_t sat_transform3d_get_world(
    const sat_transform3d_world_t* world, uint16_t id, sat_mat4_t* out) {
    if (!is_live(world, id) || !out) return SAT_ERR_INVALID_ARG;
    if (world->pending) return SAT_ERR_BUSY;
    *out = world->nodes[id].world;
    return SAT_OK;
}
