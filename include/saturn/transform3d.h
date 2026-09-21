#ifndef SATURN_TRANSFORM3D_H
#define SATURN_TRANSFORM3D_H

#include <stdint.h>
#include "saturn/scene3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Caller-owned hierarchy. IDs remain stable until reset; no node deletion,
 * hidden allocation, recursion, or dependency on the graphics hardware. */
#define SAT_TRANSFORM3D_ROOT ((uint16_t)0xffffu)

typedef struct sat_transform3d_node {
    sat_model_transform3d_t local;
    sat_mat4_t local_matrix; /* Exact local pose when use_local_matrix=1. */
    sat_mat4_t world;
    uint8_t use_local_matrix;
    uint16_t parent;
    uint8_t dirty;
    uint8_t eval_state; /* Internal evaluation scratch; do not write. */
} sat_transform3d_node_t;

typedef struct sat_transform3d_world {
    sat_transform3d_node_t* nodes;
    uint16_t count;
    uint16_t capacity;
    uint8_t pending;
} sat_transform3d_world_t;

sat_result_t sat_transform3d_world_init(
    sat_transform3d_world_t* world, sat_transform3d_node_t* nodes,
    uint16_t capacity);
void sat_transform3d_world_reset(sat_transform3d_world_t* world);

/* Creates an identity root. No IDs are reused before reset. */
sat_result_t sat_transform3d_create(
    sat_transform3d_world_t* world, uint16_t* out_id);

/* Rejects invalid parents and cycles. Reparenting preserves LOCAL coordinates,
 * not the former world-space pose. ROOT detaches the node. */
sat_result_t sat_transform3d_set_parent(
    sat_transform3d_world_t* world, uint16_t id, uint16_t parent);
sat_result_t sat_transform3d_set_local(
    sat_transform3d_world_t* world, uint16_t id,
    const sat_model_transform3d_t* local);

/* Exact row-major local matrix override for quaternion poses, authored
 * transforms, and arbitrary affine local transforms. Copies the matrix;
 * subsequent set_local() switches back to the traditional TRS descriptor.
 * Parent composition and dirty propagation are identical for both modes.
 * Caller is responsible for using affine/fixed-point-safe matrices. */
sat_result_t sat_transform3d_set_local_matrix(
    sat_transform3d_world_t* world, uint16_t id,
    const sat_mat4_t* local_matrix);

/* scratch must have >= world->count uint16 entries. Evaluates each node
 * parent-first, refreshing descendants when an ancestor changes. No heap,
 * recursion or hardware access. Results are cached until the next mutation. */
sat_result_t sat_transform3d_evaluate(
    sat_transform3d_world_t* world, uint16_t* scratch,
    uint16_t scratch_capacity);

/* Returns SAT_ERR_BUSY after any mutation until evaluate() succeeds. */
sat_result_t sat_transform3d_get_world(
    const sat_transform3d_world_t* world, uint16_t id, sat_mat4_t* out);

#ifdef __cplusplus
}
#endif
#endif
