#include "saturn/water_sim.h"

namespace {

constexpr int32_t kLimit = SAT_WATER_SIM_LIMIT;

bool valid(const sat_water_sim_t* s) {
    return s && s->height && s->velocity && s->size >= SAT_WATER_SIM_MIN_SIZE &&
           s->size <= SAT_WATER_SIM_MAX_SIZE;
}

int32_t clamp_limit(int64_t v) {
    return static_cast<int32_t>(v > kLimit ? kLimit : (v < -kLimit ? -kLimit : v));
}

/* The same encoding as sat_gouraud_rgb in saturn/vdp1.h. */
uint16_t gouraud(int dr, int dg, int db) {
    auto channel = [](int d) { d += 16; return d < 0 ? 0 : (d > 31 ? 31 : d); };
    return static_cast<uint16_t>((channel(db) << 10) | (channel(dg) << 5) | channel(dr));
}

int level(int64_t fixed_product) {
    const int64_t v = fixed_product >> 32;
    return static_cast<int>(v > 64 ? 64 : (v < -64 ? -64 : v));
}

int32_t height_at(const sat_water_sim_t& s, int x, int z) {
    const int n = s.size;
    x = x < 0 ? 0 : (x >= n ? n - 1 : x);
    z = z < 0 ? 0 : (z >= n ? n - 1 : z);
    return s.height[z * n + x];
}

/* Kernel weight in 16.16 for squared distance d2 and squared radius r2, both
 * 16.16; zero outside the radius. (1 - t^2)^2 is smooth and cheap. */
int64_t weight(int64_t d2, int64_t r2) {
    if (d2 >= r2) return 0;
    const int64_t u = SAT_FX16_ONE - d2 * SAT_FX16_ONE / r2;
    return u * u / SAT_FX16_ONE;
}

}

extern "C" sat_result_t sat_water_sim_init(sat_water_sim_t* sim, uint16_t size, uint16_t damping,
    sat_fx16_t* height, sat_fx16_t* velocity) {
    if (!sim || !height || !velocity || size < SAT_WATER_SIM_MIN_SIZE ||
        size > SAT_WATER_SIM_MAX_SIZE)
        return SAT_ERR_INVALID_ARG;
    *sim = {size, damping, height, velocity};
    sat_water_sim_reset(sim);
    return SAT_OK;
}

extern "C" void sat_water_sim_reset(sat_water_sim_t* sim) {
    if (!valid(sim)) return;
    const unsigned count = static_cast<unsigned>(sim->size) * sim->size;
    for (unsigned i = 0; i < count; ++i) sim->height[i] = sim->velocity[i] = 0;
}

extern "C" void sat_water_sim_step(sat_water_sim_t* sim) {
    if (!valid(sim)) return;
    const int n = sim->size;
    sat_fx16_t* const h = sim->height;
    sat_fx16_t* const v = sim->velocity;
    int64_t total = 0;
    // Each neighbor term is odd in the height difference, so what one node
    // gains its neighbor loses and the summed velocity stays zero before damping.
    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            const int i = z * n + x;
            int64_t force = 0;
            if (x > 0) force += (h[i - 1] - h[i]) / 4;
            if (x + 1 < n) force += (h[i + 1] - h[i]) / 4;
            if (z > 0) force += (h[i - n] - h[i]) / 4;
            if (z + 1 < n) force += (h[i + n] - h[i]) / 4;
            int32_t velocity = clamp_limit(v[i] + force);
            if (sim->damping) velocity -= velocity / sim->damping;
            v[i] = velocity;
            total += velocity;
        }
    }
    // Damping rounds per node. Remove the leftover mean so volume is exact.
    const int64_t count = static_cast<int64_t>(n) * n;
    const int64_t mean = total / count;
    int64_t remainder = total % count;
    const int step = remainder < 0 ? -1 : 1;
    if (remainder < 0) remainder = -remainder;
    for (int64_t i = 0; i < count; ++i) {
        v[i] -= static_cast<int32_t>(mean + (i < remainder ? step : 0));
        h[i] = clamp_limit(static_cast<int64_t>(h[i]) + v[i]);
    }
}

extern "C" int64_t sat_water_sim_volume(const sat_water_sim_t* sim) {
    if (!valid(sim)) return 0;
    int64_t total = 0;
    const unsigned count = static_cast<unsigned>(sim->size) * sim->size;
    for (unsigned i = 0; i < count; ++i) total += sim->height[i];
    return total;
}

extern "C" sat_result_t sat_water_sim_bump(sat_water_sim_t* sim, sat_fx16_t x, sat_fx16_t z,
    sat_fx16_t radius, sat_fx16_t amount) {
    if (!valid(sim) || radius <= 0) return SAT_ERR_INVALID_ARG;
    if (amount == 0) return SAT_OK;
    const int n = sim->size;
    const int64_t r2 = static_cast<int64_t>(radius) * radius / SAT_FX16_ONE;
    const int span = static_cast<int>((static_cast<int64_t>(radius) + SAT_FX16_ONE - 1) / SAT_FX16_ONE);
    const int cx = static_cast<int>((static_cast<int64_t>(x) + SAT_FX16_ONE / 2) >> 16);
    const int cz = static_cast<int>((static_cast<int64_t>(z) + SAT_FX16_ONE / 2) >> 16);
    const int x0 = cx - span < 0 ? 0 : cx - span, x1 = cx + span >= n ? n - 1 : cx + span;
    const int z0 = cz - span < 0 ? 0 : cz - span, z1 = cz + span >= n ? n - 1 : cz + span;
    int64_t total = 0, best = 0;
    int best_x = -1, best_z = -1;
    for (int j = z0; j <= z1; ++j) {
        for (int i = x0; i <= x1; ++i) {
            const int64_t dx = (static_cast<int64_t>(i) << 16) - x;
            const int64_t dz = (static_cast<int64_t>(j) << 16) - z;
            const int64_t w = weight((dx * dx + dz * dz) / SAT_FX16_ONE, r2);
            total += w;
            if (w > best) { best = w; best_x = i; best_z = j; }
        }
    }
    if (total == 0) return SAT_OK;
    int64_t given = 0;
    for (int j = z0; j <= z1; ++j) {
        for (int i = x0; i <= x1; ++i) {
            const int64_t dx = (static_cast<int64_t>(i) << 16) - x;
            const int64_t dz = (static_cast<int64_t>(j) << 16) - z;
            const int64_t w = weight((dx * dx + dz * dz) / SAT_FX16_ONE, r2);
            const int64_t share = static_cast<int64_t>(amount) * w / total;
            given += share;
            sim->height[j * n + i] = clamp_limit(sim->height[j * n + i] + share);
        }
    }
    sim->height[best_z * n + best_x] = clamp_limit(
        sim->height[best_z * n + best_x] + (static_cast<int64_t>(amount) - given));
    return SAT_OK;
}

extern "C" sat_result_t sat_water_sim_displace(sat_water_sim_t* sim,
    sat_fx16_t from_x, sat_fx16_t from_z, sat_fx16_t to_x, sat_fx16_t to_z,
    sat_fx16_t radius, sat_fx16_t amount) {
    const sat_result_t first = sat_water_sim_bump(sim, from_x, from_z, radius, amount);
    if (first != SAT_OK) return first;
    return sat_water_sim_bump(sim, to_x, to_z, radius, -amount);
}

extern "C" sat_result_t sat_water_sim_sample(const sat_water_sim_t* sim, sat_fx16_t x, sat_fx16_t z,
    sat_fx16_t* out) {
    if (!valid(sim) || !out) return SAT_ERR_INVALID_ARG;
    const int64_t max = (static_cast<int64_t>(sim->size) - 1) << 16;
    const int64_t px = x < 0 ? 0 : (x > max ? max : x);
    const int64_t pz = z < 0 ? 0 : (z > max ? max : z);
    const int ix = static_cast<int>(px >> 16), iz = static_cast<int>(pz >> 16);
    const int64_t fx = px & 0xFFFF, fz = pz & 0xFFFF;
    const int64_t top = height_at(*sim, ix, iz) * (65536 - fx) + static_cast<int64_t>(height_at(*sim, ix + 1, iz)) * fx;
    const int64_t bottom = height_at(*sim, ix, iz + 1) * (65536 - fx) + static_cast<int64_t>(height_at(*sim, ix + 1, iz + 1)) * fx;
    *out = static_cast<sat_fx16_t>((top / 65536 * (65536 - fz) + bottom / 65536 * fz) / 65536);
    return SAT_OK;
}

extern "C" sat_result_t sat_water_sim_slope(const sat_water_sim_t* sim, uint16_t x, uint16_t z,
    sat_fx16_t* slope_x, sat_fx16_t* slope_z) {
    if (!valid(sim) || x >= sim->size || z >= sim->size || !slope_x || !slope_z)
        return SAT_ERR_INVALID_ARG;
    *slope_x = height_at(*sim, x + 1, z) - height_at(*sim, x - 1, z);
    *slope_z = height_at(*sim, x, z + 1) - height_at(*sim, x, z - 1);
    return SAT_OK;
}

extern "C" sat_result_t sat_water_sim_shade(const sat_water_sim_t* sim,
    const sat_water_sim_shading_t* shading, uint16_t* out) {
    if (!valid(sim) || !shading || shading->spacing <= 0 || !out) return SAT_ERR_INVALID_ARG;
    const int n = sim->size;
    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            // Edge nodes use one-sided differences, so their span is one node.
            const int span_x = (x > 0 && x + 1 < n) ? 2 : 1;
            const int span_z = (z > 0 && z + 1 < n) ? 2 : 1;
            const int64_t dx = height_at(*sim, x + 1, z) - height_at(*sim, x - 1, z);
            const int64_t dz = height_at(*sim, x, z + 1) - height_at(*sim, x, z - 1);
            const int64_t sx = dx * SAT_FX16_ONE / (static_cast<int64_t>(shading->spacing) * span_x);
            const int64_t sz = dz * SAT_FX16_ONE / (static_cast<int64_t>(shading->spacing) * span_z);
            const int64_t light = (sx * shading->light_x + sz * shading->light_z) / SAT_FX16_ONE;
            const int64_t magnitude = (sx < 0 ? -sx : sx) + (sz < 0 ? -sz : sz);
            int d[3];
            for (int c = 0; c < 3; ++c)
                d[c] = level(light * shading->light_gain[c] + magnitude * shading->sky_gain[c]);
            out[z * n + x] = gouraud(d[0], d[1], d[2]);
        }
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_water_sim_caustics(const sat_water_sim_t* sim, int32_t gain, uint16_t* out) {
    if (!valid(sim) || !out) return SAT_ERR_INVALID_ARG;
    const int n = sim->size;
    for (int z = 0; z < n; ++z) {
        for (int x = 0; x < n; ++x) {
            const int64_t laplacian = static_cast<int64_t>(height_at(*sim, x - 1, z)) +
                height_at(*sim, x + 1, z) + height_at(*sim, x, z - 1) +
                height_at(*sim, x, z + 1) - 4 * static_cast<int64_t>(height_at(*sim, x, z));
            const int d = level(-laplacian * gain);
            out[z * n + x] = gouraud(d, d, d);
        }
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_water_sim_sample_volume(const sat_water_sim_t* sim,
    const sat_water_volume_t* volume, const sat_vec3_t* origin, sat_fx16_t spacing,
    const sat_vec3_t* base, sat_fx16_t height, sat_water_sample_t* out) {
    if (!valid(sim) || !volume || !origin || spacing <= 0 || !base || !out)
        return SAT_ERR_INVALID_ARG;
    const int64_t nx = (static_cast<int64_t>(base->x) - origin->x) * SAT_FX16_ONE / spacing;
    const int64_t nz = (static_cast<int64_t>(base->z) - origin->z) * SAT_FX16_ONE / spacing;
    const int64_t max = (static_cast<int64_t>(sim->size) - 1) << 16;
    sat_fx16_t wave = 0;
    sat_water_sim_sample(sim, static_cast<sat_fx16_t>(nx < 0 ? 0 : (nx > max ? max : nx)),
        static_cast<sat_fx16_t>(nz < 0 ? 0 : (nz > max ? max : nz)), &wave);
    sat_water_volume_t local = *volume;
    const int64_t surface = static_cast<int64_t>(volume->surface) + wave;
    local.surface = static_cast<sat_fx16_t>(surface > INT32_MAX ? INT32_MAX : (surface < INT32_MIN ? INT32_MIN : surface));
    return sat_water_volume_sample(&local, base, height, out);
}
