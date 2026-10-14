#pragma once
#include "saturn/math3d.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { CLOWN_LIGHT_Q8=256, CLOWN_LIGHT_SHIFT=8, CLOWN_NORMAL_LIMIT=1024,
       CLOWN_CONE_TAN_Q8=72, CLOWN_LIGHT_RANGE_Q8=8*CLOWN_LIGHT_Q8 };
typedef struct { sat_vec3_t source,target,axis,u,v; } clown_spotlight_t;
sat_vec3_t clown_light_normalize(sat_vec3_t vector);
void clown_spotlight_aim(clown_spotlight_t* light,sat_vec3_t source,sat_vec3_t target);
unsigned clown_spotlight_strength(const clown_spotlight_t* light,sat_vec3_t point);
unsigned clown_spotlight_diffuse(const clown_spotlight_t* light,sat_vec3_t point,sat_vec3_t normal);
/* Cone-rim ray intersected with Y=0, or clipped at the light's range. */
int clown_spotlight_ground(const clown_spotlight_t* light,sat_fx16_t angle,sat_vec3_t* point);
#ifdef __cplusplus
}
#endif
