#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include "saturn/water_sim.h"

namespace {

constexpr sat_fx16_t one = SAT_FX16_ONE;

bool near(sat_fx16_t a, sat_fx16_t b) { return (a > b ? a - b : b - a) <= 16; }

struct Pool {
    sat_fx16_t height[16 * 16];
    sat_fx16_t velocity[16 * 16];
    sat_water_sim_t sim;
    explicit Pool(uint16_t size = 16, uint16_t damping = 256) {
        assert(sat_water_sim_init(&sim, size, damping, height, velocity) == SAT_OK);
    }
    sat_fx16_t& at(int x, int z) { return height[z * sim.size + x]; }
};

void init_validation() {
    sat_fx16_t h[16 * 16], v[16 * 16];
    sat_water_sim_t sim;
    assert(sat_water_sim_init(nullptr, 8, 256, h, v) == SAT_ERR_INVALID_ARG);
    assert(sat_water_sim_init(&sim, 2, 256, h, v) == SAT_ERR_INVALID_ARG);
    assert(sat_water_sim_init(&sim, SAT_WATER_SIM_MAX_SIZE + 1, 256, h, v) == SAT_ERR_INVALID_ARG);
    assert(sat_water_sim_init(&sim, 8, 256, nullptr, v) == SAT_ERR_INVALID_ARG);
    assert(sat_water_sim_init(&sim, 8, 256, h, v) == SAT_OK && sat_water_sim_volume(&sim) == 0);
}

void rest_stays_at_rest() {
    Pool pool;
    for (int i = 0; i < 100; ++i) sat_water_sim_step(&pool.sim);
    for (sat_fx16_t h : pool.height) assert(h == 0);
}

void volume_is_exact() {
    Pool pool;
    assert(sat_water_sim_bump(&pool.sim, 7 * one + one / 3, 5 * one + 1, 3 * one, one) == SAT_OK);
    assert(sat_water_sim_volume(&pool.sim) == one);
    // Near an edge the kernel is cut but exactly the requested amount arrives.
    assert(sat_water_sim_bump(&pool.sim, 0, 0, 4 * one, 5 * one) == SAT_OK);
    const int64_t volume = sat_water_sim_volume(&pool.sim);
    assert(volume == 6 * int64_t(one));
    for (int i = 0; i < 2000; ++i) {
        sat_water_sim_step(&pool.sim);
        assert(sat_water_sim_volume(&pool.sim) == volume);
    }
}

void displacement_conserves() {
    Pool pool;
    assert(sat_water_sim_displace(&pool.sim, 4 * one, 4 * one, 9 * one + 7, 6 * one, 2 * one, one) == SAT_OK);
    assert(sat_water_sim_volume(&pool.sim) == 0);
    bool raised = false, lowered = false;
    for (sat_fx16_t h : pool.height) { raised |= h > 0; lowered |= h < 0; }
    assert(raised && lowered);
}

void drop_spreads_symmetrically_and_decays() {
    Pool pool(15, 256);
    assert(sat_water_sim_bump(&pool.sim, 7 * one, 7 * one, 3 * one, 4 * one) == SAT_OK);
    for (int i = 0; i < 6; ++i) {
        sat_water_sim_step(&pool.sim);
        for (int z = 0; z < 15; ++z)
            for (int x = 0; x < 15; ++x) {
                const sat_fx16_t centre = pool.height[z * 15 + x];
                // The mean correction hands a +-1 rounding remainder to the first
                // nodes each step, so symmetry holds to a few raw units.
                assert(near(centre, pool.height[x * 15 + z]));
                assert(near(centre, pool.height[z * 15 + (14 - x)]));
                assert(near(centre, pool.height[(14 - z) * 15 + x]));
            }
    }
    // The ripple has left the middle by now and is bounded.
    int32_t peak = 0;
    for (int i = 0; i < 4000; ++i) {
        sat_water_sim_step(&pool.sim);
        for (int k = 0; k < 15 * 15; ++k) {
            const int32_t a = pool.height[k] < 0 ? -pool.height[k] : pool.height[k];
            if (a > peak) peak = a;
        }
    }
    assert(peak < 4 * one);
    for (int k = 0; k < 15 * 15; ++k) {
        const int32_t a = pool.height[k] < 0 ? -pool.height[k] : pool.height[k];
        assert(a < one / 16);  // damped far below the initial bump
    }
}

void sample_and_slope() {
    Pool pool;
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) pool.at(x, z) = x * 100;
    sat_fx16_t value = 0, sx = 0, sz = 0;
    assert(sat_water_sim_sample(&pool.sim, 2 * one + one / 2, 3 * one, &value) == SAT_OK && value == 250);
    assert(sat_water_sim_sample(&pool.sim, -5 * one, 3 * one, &value) == SAT_OK && value == 0);
    assert(sat_water_sim_sample(&pool.sim, 99 * one, 99 * one, &value) == SAT_OK && value == 1500);
    assert(sat_water_sim_slope(&pool.sim, 5, 5, &sx, &sz) == SAT_OK && sx == 200 && sz == 0);
    assert(sat_water_sim_slope(&pool.sim, 0, 5, &sx, &sz) == SAT_OK && sx == 100);
    assert(sat_water_sim_slope(&pool.sim, 16, 5, &sx, &sz) == SAT_ERR_INVALID_ARG);
}

void shading() {
    Pool pool;
    for (int z = 0; z < 16; ++z)
        for (int x = 0; x < 16; ++x) pool.at(x, z) = x * (one / 4);
    sat_water_sim_shading_t shading{};
    shading.spacing = one;
    shading.light_x = one;
    shading.light_gain[0] = shading.light_gain[1] = shading.light_gain[2] = 16 * one;
    uint16_t colors[16 * 16];
    assert(sat_water_sim_shade(&pool.sim, &shading, colors) == SAT_OK);
    // Slope 0.25 toward the light: +4 levels per channel around neutral (16).
    assert(colors[5 * 16 + 5] == (uint16_t)((20 << 10) | (20 << 5) | 20));
    shading.light_x = -one;
    assert(sat_water_sim_shade(&pool.sim, &shading, colors) == SAT_OK);
    assert(colors[5 * 16 + 5] == (uint16_t)((12 << 10) | (12 << 5) | 12));
    shading.spacing = 0;
    assert(sat_water_sim_shade(&pool.sim, &shading, colors) == SAT_ERR_INVALID_ARG);

    Pool crest;
    crest.at(8, 8) = one / 4;
    // Laplacian at the crest is -1 unit; focus = +gain levels. Neighbours see +0.25.
    assert(sat_water_sim_caustics(&crest.sim, 4 * one, colors) == SAT_OK);
    assert(colors[8 * 16 + 8] == (uint16_t)((20 << 10) | (20 << 5) | 20));
    assert(colors[8 * 16 + 9] == (uint16_t)((15 << 10) | (15 << 5) | 15));
    assert(colors[2 * 16 + 2] == 0x4210u);
    assert(sat_water_sim_caustics(&crest.sim, 0, colors) == SAT_OK);
    for (uint16_t c : colors) assert(c == 0x4210u);
}

void follows_waves() {
    Pool pool;
    sat_water_volume_t volume{};
    volume.min_x = -8 * one; volume.max_x = 8 * one;
    volume.min_z = -8 * one; volume.max_z = 8 * one;
    volume.bottom = -4 * one; volume.surface = 0;
    const sat_vec3_t origin = {-8 * one, 0, -8 * one};
    pool.at(8, 8) = one / 2;  // world (0, 0)
    sat_water_sample_t sample;
    const sat_vec3_t above = {0, one / 4, 0};
    assert(sat_water_sim_sample_volume(&pool.sim, &volume, &origin, one, &above, one / 2, &sample) == SAT_OK);
    assert(sample.surface == one / 2 && sample.inside && sample.fraction > 0);
    const sat_vec3_t away = {-7 * one, one / 4, -7 * one};
    assert(sat_water_sim_sample_volume(&pool.sim, &volume, &origin, one, &away, one / 2, &sample) == SAT_OK);
    assert(sample.surface == 0 && sample.fraction == 0);
    assert(sat_water_sim_sample_volume(&pool.sim, &volume, &origin, 0, &away, 0, &sample) == SAT_ERR_INVALID_ARG);
}

constexpr uint64_t PINNED_HASH = 6420950037558331274ull;

uint64_t fnv(const Pool& pool, uint64_t hash) {
    const unsigned count = pool.sim.size * pool.sim.size;
    for (unsigned i = 0; i < count; ++i)
        for (sat_fx16_t value : {pool.height[i], pool.velocity[i]})
            for (unsigned shift = 0; shift < 32; shift += 8)
                hash = (hash ^ ((static_cast<uint32_t>(value) >> shift) & 0xFFu)) * 1099511628211ull;
    return hash;
}

/* A fixed scripted run pinned bit for bit: drops, a moving body and 120 steps
 * on the 9x9 grid. A change to the numerics must change this on purpose. */
void pinned_state() {
    Pool pool(9, 256);
    uint64_t hash = 14695981039346656037ull;
    for (int tick = 0; tick < 120; ++tick) {
        if (tick == 5) assert(sat_water_sim_bump(&pool.sim, 3 * one, 4 * one, 2 * one, 3 * one) == SAT_OK);
        if (tick >= 20 && tick < 40)
            assert(sat_water_sim_displace(&pool.sim, (tick - 20) * (one / 4) + one, 5 * one,
                (tick - 19) * (one / 4) + one, 5 * one, 2 * one, one) == SAT_OK);
        sat_water_sim_step(&pool.sim);
        if (tick % 40 == 39) hash = fnv(pool, hash);
    }
    assert(sat_water_sim_volume(&pool.sim) == 3 * int64_t(one));
#ifdef PRINT_PINNED_HASH
    std::printf("%llu\n", static_cast<unsigned long long>(hash));
#else
    assert(hash == PINNED_HASH);
#endif
}

}

int main() {
    pinned_state();
    init_validation();
    rest_stays_at_rest();
    volume_is_exact();
    displacement_conserves();
    drop_spreads_symmetrically_and_decays();
    sample_and_slope();
    shading();
    follows_waves();
}
