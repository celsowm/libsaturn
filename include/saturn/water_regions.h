#ifndef SATURN_WATER_REGIONS_H
#define SATURN_WATER_REGIONS_H

#include "saturn/water_volume.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Several water boxes in one level, each a sat_water_volume_t with its own
 * surface that the game may raise or lower. Storage is caller-owned and the
 * array index is the region id. */
#define SAT_WATER_REGION_NONE 0xFFFFu

typedef struct sat_water_regions {
    sat_water_volume_t* volumes;
    uint16_t count;
} sat_water_regions_t;

/* Validates every volume in `storage` (the same rules as
 * sat_water_volume_sample) and binds it. */
sat_result_t sat_water_regions_init(sat_water_regions_t* regions,
    sat_water_volume_t* storage, uint16_t count);

/* Moves one region's surface; it must stay above the region's bottom. */
sat_result_t sat_water_regions_set_surface(sat_water_regions_t* regions,
    uint16_t index, sat_fx16_t surface);

/* The region whose horizontal bounds contain the point and whose bottom lies
 * at or below it: the water the point is over or in, for choosing a plane.
 * Among overlapping regions the highest surface wins, then the lowest index.
 * SAT_WATER_REGION_NONE when there is none. */
sat_result_t sat_water_regions_find(const sat_water_regions_t* regions,
    const sat_vec3_t* point, uint16_t* out_index);

/* sat_water_volume_sample over every region. The region with the largest
 * submerged fraction answers (lowest index on a tie); with none, the sample
 * is zero and the index SAT_WATER_REGION_NONE. */
sat_result_t sat_water_regions_sample(const sat_water_regions_t* regions,
    const sat_vec3_t* base, sat_fx16_t height, sat_water_sample_t* out,
    uint16_t* out_index);

/* sat_water_volume_immersion over every region, for a point such as the
 * camera. `was_region` is the result of the previous call (or NONE); its
 * region uses the exit height, the others the entry height. The highest
 * surface among the regions the point is under wins. */
sat_result_t sat_water_regions_immersion(const sat_water_regions_t* regions,
    const sat_vec3_t* point, sat_fx16_t hysteresis, uint16_t was_region,
    uint16_t* out_region);

#ifdef __cplusplus
}
#endif
#endif
