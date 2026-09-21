#ifndef SATURN_PHYSICS3_TRANSFORM_H
#define SATURN_PHYSICS3_TRANSFORM_H
#include "saturn/physics3_world.h"
#include "saturn/transform3d.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One-way, explicitly invoked bridge from an already stepped physics
 * sphere to a ROOT transform node. Its exact quaternion pose is copied as
 * a local matrix: no Euler conversion, heap allocation or persistent binding.
 * Rejects parented nodes, whose local pose cannot equal world-space physics
 * without computing the inverse parent matrix. Caller then evaluates the
 * hierarchy once and submits the node with sat_scene_submit_transform_instance.
 * Child nodes inherit the synchronized pose. */
sat_result_t sat_physics3_sync_sphere_transform(
    const sat_physics3_world_t* physics, uint16_t sphere_id,
    sat_transform3d_world_t* transforms, uint16_t node_id);
#ifdef __cplusplus
}
#endif
#endif
