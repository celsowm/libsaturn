#include "saturn/water_regions.h"

namespace {

bool valid_volume(const sat_water_volume_t& v) {
    return v.min_x < v.max_x && v.min_z < v.max_z && v.bottom < v.surface &&
           v.buoyancy >= 0 && v.drag >= 0 && v.drag <= SAT_FX16_ONE;
}

bool valid(const sat_water_regions_t* r) { return r && (r->count == 0 || r->volumes); }

bool over(const sat_water_volume_t& v, const sat_vec3_t& p) {
    return p.x >= v.min_x && p.x <= v.max_x && p.z >= v.min_z && p.z <= v.max_z &&
           p.y >= v.bottom;
}

}

extern "C" sat_result_t sat_water_regions_init(sat_water_regions_t* regions,
    sat_water_volume_t* storage, uint16_t count) {
    if (!regions || (count && !storage) || count >= SAT_WATER_REGION_NONE)
        return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < count; ++i)
        if (!valid_volume(storage[i])) return SAT_ERR_INVALID_ARG;
    *regions = {storage, count};
    return SAT_OK;
}

extern "C" sat_result_t sat_water_regions_set_surface(sat_water_regions_t* regions,
    uint16_t index, sat_fx16_t surface) {
    if (!valid(regions) || index >= regions->count || surface <= regions->volumes[index].bottom)
        return SAT_ERR_INVALID_ARG;
    regions->volumes[index].surface = surface;
    return SAT_OK;
}

extern "C" sat_result_t sat_water_regions_find(const sat_water_regions_t* regions,
    const sat_vec3_t* point, uint16_t* out_index) {
    if (!valid(regions) || !point || !out_index) return SAT_ERR_INVALID_ARG;
    uint16_t best = SAT_WATER_REGION_NONE;
    for (uint16_t i = 0; i < regions->count; ++i) {
        if (!over(regions->volumes[i], *point)) continue;
        if (best == SAT_WATER_REGION_NONE || regions->volumes[i].surface > regions->volumes[best].surface)
            best = i;
    }
    *out_index = best;
    return SAT_OK;
}

extern "C" sat_result_t sat_water_regions_sample(const sat_water_regions_t* regions,
    const sat_vec3_t* base, sat_fx16_t height, sat_water_sample_t* out, uint16_t* out_index) {
    if (!valid(regions) || !base || height < 0 || !out || !out_index) return SAT_ERR_INVALID_ARG;
    sat_water_sample_t best{};
    uint16_t found = SAT_WATER_REGION_NONE;
    for (uint16_t i = 0; i < regions->count; ++i) {
        sat_water_sample_t sample;
        const sat_result_t r = sat_water_volume_sample(&regions->volumes[i], base, height, &sample);
        if (r != SAT_OK) return r;
        if (!sample.inside) continue;
        if (found == SAT_WATER_REGION_NONE || sample.fraction > best.fraction) {
            best = sample;
            found = i;
        }
    }
    *out = best;
    *out_index = found;
    return SAT_OK;
}

extern "C" sat_result_t sat_water_regions_immersion(const sat_water_regions_t* regions,
    const sat_vec3_t* point, sat_fx16_t hysteresis, uint16_t was_region, uint16_t* out_region) {
    if (!valid(regions) || !point || hysteresis < 0 || !out_region) return SAT_ERR_INVALID_ARG;
    uint16_t best = SAT_WATER_REGION_NONE;
    for (uint16_t i = 0; i < regions->count; ++i) {
        uint8_t under = 0;
        const sat_result_t r = sat_water_volume_immersion(&regions->volumes[i], point, hysteresis,
                                                          i == was_region, &under);
        if (r != SAT_OK) return r;
        if (!under) continue;
        if (best == SAT_WATER_REGION_NONE || regions->volumes[i].surface > regions->volumes[best].surface)
            best = i;
    }
    *out_region = best;
    return SAT_OK;
}
