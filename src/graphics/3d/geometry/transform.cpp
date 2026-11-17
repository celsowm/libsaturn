#include "saturn/transform3d.h"
#include "src/graphics/3d/geometry/hierarchy_evaluate.hpp"
#if defined(SAT_HIERARCHY_EXPERIMENT) && SAT_HIERARCHY_EXPERIMENT
#include "src/graphics/3d/geometry/affine_compose_sh2.hpp"
#include "saturn/time.h"
extern "C" {
volatile uint32_t g_hierarchy_experiment_ticks;
extern const uint32_t g_hierarchy_experiment_mode=SAT_HIERARCHY_EXPERIMENT;
}
#endif

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
    node.local_matrix=node.world;
    node.use_local_matrix=0;
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
    world->nodes[id].use_local_matrix=0;
    world->nodes[id].dirty = 1;
    world->pending = 1;
    return SAT_OK;
}

extern "C" sat_result_t sat_transform3d_set_local_matrix(
    sat_transform3d_world_t* world,uint16_t id,
    const sat_mat4_t* local_matrix){
    if(!is_live(world,id)||!local_matrix)return SAT_ERR_INVALID_ARG;
    /* The hierarchy already operates on affine model matrices. Guard the
     * invariant that the final row remains the homogeneous affine row. */
    if(local_matrix->m[12]||local_matrix->m[13]||
       local_matrix->m[14]||local_matrix->m[15]!=SAT_FX16_ONE)
        return SAT_ERR_INVALID_ARG;
    sat_transform3d_node_t& node=world->nodes[id];
    node.local_matrix=*local_matrix;
    node.use_local_matrix=1;
    node.dirty=1;
    world->pending=1;
    return SAT_OK;
}
extern "C" sat_result_t sat_transform3d_evaluate(
    sat_transform3d_world_t* world, uint16_t* scratch,
    uint16_t scratch_capacity) {
#if defined(SAT_HIERARCHY_EXPERIMENT) && SAT_HIERARCHY_EXPERIMENT
    const uint16_t start=sat_time_frc();
#if SAT_HIERARCHY_EXPERIMENT == 2
    const auto result=saturn::core::hierarchy::evaluate(world,scratch,scratch_capacity,saturn::core::hierarchy::compose_cpp);
#elif SAT_HIERARCHY_EXPERIMENT == 3
    const auto result=saturn::core::hierarchy::evaluate(world,scratch,scratch_capacity,saturn::core::hierarchy::compose_asm);
#else
    const auto result=saturn::core::hierarchy::evaluate(world,scratch,scratch_capacity,sat_mat4_multiply);
#endif
    g_hierarchy_experiment_ticks=uint16_t(sat_time_frc()-start);
    return result;
#else
    return saturn::core::hierarchy::evaluate(world,scratch,scratch_capacity,sat_mat4_multiply);
#endif
}

extern "C" sat_result_t sat_transform3d_get_world(
    const sat_transform3d_world_t* world, uint16_t id, sat_mat4_t* out) {
    if (!is_live(world, id) || !out) return SAT_ERR_INVALID_ARG;
    if (world->pending) return SAT_ERR_BUSY;
    *out = world->nodes[id].world;
    return SAT_OK;
}
