#ifndef SATURN_PHYSICS3_WORLD_H
#define SATURN_PHYSICS3_WORLD_H
#include <stdint.h>
#include "saturn/collide3d.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Fixed-tick, caller-owned sphere/box physics. No heap or graphics dependency. */
typedef enum sat_physics3_kind {
    SAT_PHYSICS3_STATIC_BOX=1,
    SAT_PHYSICS3_KINEMATIC_BOX=2,
    SAT_PHYSICS3_DYNAMIC_SPHERE=3
} sat_physics3_kind_t;
typedef struct sat_physics3_material {
    sat_fx16_t friction;    /* [0,ONE]; tangent damping on support */
    sat_fx16_t restitution; /* [0,ONE]; normal bounce */
} sat_physics3_material_t;
typedef struct sat_physics3_actor {
    sat_physics3_kind_t kind;
    sat_physics3_material_t material;
    sat_body3_t sphere;
    sat_aabb3_t box;
    sat_vec3_t target_center; /* kinematic target at end of NEXT tick */
    sat_vec3_t frame_motion;  /* displacement per tick, zero for static boxes */
} sat_physics3_actor_t;
typedef struct sat_physics3_world {
    sat_physics3_actor_t* actors;
    sat_vec3_t gravity; /* velocity delta per fixed tick */
    uint16_t count,capacity;
    uint8_t max_substeps; /* 1..64 */
    uint8_t iterations; /* 1..8 */
} sat_physics3_world_t;
sat_result_t sat_physics3_world_init(sat_physics3_world_t* world,
    sat_physics3_actor_t* storage,uint16_t capacity,sat_vec3_t gravity,
    uint8_t max_substeps,uint8_t iterations);
void sat_physics3_world_reset(sat_physics3_world_t* world);
sat_result_t sat_physics3_add_box(sat_physics3_world_t* world,
    sat_physics3_kind_t kind,const sat_aabb3_t* box,
    const sat_physics3_material_t* material,uint16_t* out_id);
sat_result_t sat_physics3_add_sphere(sat_physics3_world_t* world,
    const sat_sphere_t* sphere,const sat_vec3_t* velocity,
    const sat_physics3_material_t* material,uint16_t* out_id);
sat_result_t sat_physics3_set_kinematic_target(sat_physics3_world_t* world,
    uint16_t id,const sat_vec3_t* center);
sat_result_t sat_physics3_set_velocity(sat_physics3_world_t* world,
    uint16_t id,const sat_vec3_t* velocity);
sat_result_t sat_physics3_get_actor(const sat_physics3_world_t* world,
    uint16_t id,sat_physics3_actor_t* out);
/* Preflights velocity/step capacity before changing ANY actor; uses existing
 * sat_sphere_aabb3_contact for iterative collision with all static/kin boxes.
 * No sphere/sphere or mesh collision and no exact swept CCD in this slice. */
sat_result_t sat_physics3_world_step(sat_physics3_world_t* world);
#ifdef __cplusplus
}
#endif
#endif
