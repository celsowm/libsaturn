#include "saturn/water_volume.h"

namespace {

bool valid(const sat_water_volume_t* v) {
    return v && v->min_x < v->max_x && v->min_z < v->max_z &&
           v->bottom < v->surface && v->buoyancy >= 0 &&
           v->drag >= 0 && v->drag <= SAT_FX16_ONE;
}

bool horizontal(const sat_water_volume_t& v, const sat_vec3_t& p) {
    return p.x >= v.min_x && p.x <= v.max_x && p.z >= v.min_z && p.z <= v.max_z;
}

sat_fx16_t saturate(int64_t value) {
    if (value > INT32_MAX) return INT32_MAX;
    if (value < INT32_MIN) return INT32_MIN;
    return static_cast<sat_fx16_t>(value);
}

int64_t damp(sat_fx16_t velocity, sat_fx16_t flow, sat_fx16_t drag) {
    const int64_t delta = static_cast<int64_t>(flow) - velocity;
    return velocity + delta * drag / SAT_FX16_ONE;
}

}

extern "C" sat_result_t sat_water_volume_sample(const sat_water_volume_t* v,
    const sat_vec3_t* base, sat_fx16_t height, sat_water_sample_t* out) {
    if (!valid(v) || !base || height < 0 || !out) return SAT_ERR_INVALID_ARG;
    sat_water_sample_t sample{};
    sample.surface = v->surface;
    const int64_t top = static_cast<int64_t>(base->y) + height;
    if (horizontal(*v, *base) && base->y <= v->surface && top >= v->bottom) {
        sample.inside = 1;
        sample.depth = saturate(static_cast<int64_t>(v->surface) - base->y);
        const int64_t low = base->y < v->bottom ? v->bottom : base->y;
        const int64_t high = top > v->surface ? v->surface : top;
        sample.fraction = height == 0
            ? (base->y < v->surface ? SAT_FX16_ONE : 0)
            : static_cast<sat_fx16_t>((high - low) * SAT_FX16_ONE / height);
    }
    *out = sample;
    return SAT_OK;
}

extern "C" sat_result_t sat_water_volume_apply(const sat_water_volume_t* v,
    const sat_water_sample_t* sample, sat_vec3_t* velocity) {
    if (!valid(v) || !sample || !velocity ||
        sample->fraction < 0 || sample->fraction > SAT_FX16_ONE)
        return SAT_ERR_INVALID_ARG;
    const sat_fx16_t fraction = sample->fraction;
    if (fraction == 0) return SAT_OK;
    const sat_fx16_t drag = static_cast<sat_fx16_t>(
        static_cast<int64_t>(fraction) * v->drag / SAT_FX16_ONE);
    const int64_t lift = static_cast<int64_t>(fraction) * v->buoyancy / SAT_FX16_ONE;
    *velocity = {saturate(damp(velocity->x, v->current_x, drag)),
                 saturate(damp(velocity->y, 0, drag) + lift),
                 saturate(damp(velocity->z, v->current_z, drag))};
    return SAT_OK;
}

extern "C" sat_result_t sat_water_volume_immersion(const sat_water_volume_t* v,
    const sat_vec3_t* point, sat_fx16_t hysteresis, uint8_t was_underwater,
    uint8_t* underwater) {
    if (!valid(v) || !point || hysteresis < 0 || !underwater) return SAT_ERR_INVALID_ARG;
    const int64_t threshold = static_cast<int64_t>(v->surface) +
        (was_underwater ? hysteresis : -hysteresis);
    *underwater = horizontal(*v, *point) && point->y >= v->bottom && point->y < threshold;
    return SAT_OK;
}
