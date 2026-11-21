#ifndef SATURN_WATER_SIM_H
#define SATURN_WATER_SIM_H

#include "saturn/water_volume.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A square height-field wave simulation on caller-owned storage. It has no
 * graphics dependency and allocates nothing. Heights are fixed-point offsets
 * from the rest level, in world units; node (x, z) is at index z * size + x.
 * Node coordinates passed to the functions below are 16.16 fixed point in
 * node units, so node 0 is at 0 and the last node is at (size - 1) << 16.
 *
 * Each step exchanges height between adjacent nodes through an odd-symmetric
 * flux with reflective edges, damps velocity, and removes the velocity mean.
 * The summed height (the water volume) is therefore unchanged by a step
 * except where a value saturates at SAT_WATER_SIM_LIMIT. */
#define SAT_WATER_SIM_MIN_SIZE 3u
#define SAT_WATER_SIM_MAX_SIZE 64u
#define SAT_WATER_SIM_LIMIT (SAT_FX16_ONE * 256)

typedef struct sat_water_sim {
    uint16_t size;
    uint16_t damping;     /* velocity loses 1/damping per step; 0 disables */
    sat_fx16_t* height;   /* size * size entries */
    sat_fx16_t* velocity; /* size * size entries */
} sat_water_sim_t;

/* Both arrays must hold size * size entries. Clears them. damping 256 is a
 * good default. */
sat_result_t sat_water_sim_init(sat_water_sim_t* sim, uint16_t size, uint16_t damping,
    sat_fx16_t* height, sat_fx16_t* velocity);
void sat_water_sim_reset(sat_water_sim_t* sim);
void sat_water_sim_step(sat_water_sim_t* sim);

/* Sum of every node height. Constant across steps (see above). */
int64_t sat_water_sim_volume(const sat_water_sim_t* sim);

/* Add `amount` (a signed volume in height units summed over nodes) around a
 * point, spread over nodes within `radius` with a smooth kernel. Exactly
 * `amount` is added, even where the kernel is cut by an edge. A zero amount or
 * a point and radius that reach no node changes nothing. */
sat_result_t sat_water_sim_bump(sat_water_sim_t* sim, sat_fx16_t x, sat_fx16_t z,
    sat_fx16_t radius, sat_fx16_t amount);

/* A moving body: raise the water where it was, lower it where it is. `amount`
 * is the displaced volume. Net volume is unchanged. */
sat_result_t sat_water_sim_displace(sat_water_sim_t* sim,
    sat_fx16_t from_x, sat_fx16_t from_z, sat_fx16_t to_x, sat_fx16_t to_z,
    sat_fx16_t radius, sat_fx16_t amount);

/* Bilinear height at a node coordinate, clamped to the grid. */
sat_result_t sat_water_sim_sample(const sat_water_sim_t* sim, sat_fx16_t x, sat_fx16_t z,
    sat_fx16_t* out);

/* Central height difference across a node, in height units per node: the
 * slope before dividing by the node spacing. One-sided at the edges. */
sat_result_t sat_water_sim_slope(const sat_water_sim_t* sim, uint16_t x, uint16_t z,
    sat_fx16_t* slope_x, sat_fx16_t* slope_z);

/* Vertex shading as VDP1 Gouraud entries (RGB555 offsets around neutral, the
 * encoding sat_gouraud_rgb produces), one per node. */
typedef struct sat_water_sim_shading {
    sat_fx16_t spacing;      /* world units per node, > 0 */
    sat_fx16_t light_x, light_z;
    int32_t light_gain[3];   /* levels per unit of light-facing slope, R G B (16.16) */
    int32_t sky_gain[3];     /* levels per unit of slope magnitude, R G B (16.16) */
} sat_water_sim_shading_t;

sat_result_t sat_water_sim_shade(const sat_water_sim_t* sim,
    const sat_water_sim_shading_t* shading, uint16_t* out);

/* Floor light for straight-down illumination: troughs spread light and
 * crests focus it, so brightness follows the negated Laplacian. `gain` is
 * levels per height unit (16.16); the result is a grey Gouraud entry. */
sat_result_t sat_water_sim_caustics(const sat_water_sim_t* sim, int32_t gain, uint16_t* out);

/* sat_water_volume_sample with the surface following the simulated waves.
 * `origin` is the world position of node (0, 0) in x and z, and `spacing` the
 * world distance between nodes. The volume's own surface is the rest level. */
sat_result_t sat_water_sim_sample_volume(const sat_water_sim_t* sim,
    const sat_water_volume_t* volume, const sat_vec3_t* origin, sat_fx16_t spacing,
    const sat_vec3_t* base, sat_fx16_t height, sat_water_sample_t* out);

#ifdef __cplusplus
}
#endif
#endif
