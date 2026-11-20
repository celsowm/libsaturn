#ifndef SATURN_WATER_VOLUME_H
#define SATURN_WATER_VOLUME_H

#include "saturn/math3d.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sat_water_volume {
    sat_fx16_t min_x, max_x, min_z, max_z;
    sat_fx16_t bottom, surface;
    sat_fx16_t current_x, current_z;     /* x/z units per tick */
    sat_fx16_t buoyancy;   /* upward velocity increment per tick at full immersion */
    sat_fx16_t drag;       /* fraction in [0,1] removed per tick at full immersion */
} sat_water_volume_t;

typedef struct sat_water_sample {
    sat_fx16_t surface;
    sat_fx16_t depth;      /* from the sample's lower endpoint (saturates) */
    sat_fx16_t fraction;   /* submerged share of the sample segment in [0,1] */
    uint8_t inside;        /* segment intersects the volume */
} sat_water_sample_t;

/* A vertical segment sampled at x/z. A zero-length segment samples a point.
 * Bounds are inclusive. Points at the surface have zero immersion. */
sat_result_t sat_water_volume_sample(const sat_water_volume_t* volume,
    const sat_vec3_t* base, sat_fx16_t height, sat_water_sample_t* out);

/* Apply one fixed tick of current-relative drag and upward buoyancy to a
 * velocity. Zero fraction preserves the velocity exactly. Outputs saturate. */
sat_result_t sat_water_volume_apply(const sat_water_volume_t* volume,
    const sat_water_sample_t* sample, sat_vec3_t* velocity);

/* Hysteresis for a point such as the camera. Uses the same horizontal and
 * bottom bounds, a lower entry height and a higher exit height. */
sat_result_t sat_water_volume_immersion(const sat_water_volume_t* volume,
    const sat_vec3_t* point, sat_fx16_t hysteresis, uint8_t was_underwater,
    uint8_t* underwater);

#ifdef __cplusplus
}
#endif
#endif
