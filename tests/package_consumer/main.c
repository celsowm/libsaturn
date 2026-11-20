/* Minimal Saturn program that consumes an *installed* LibSaturn package.
 *
 * It only exists to prove the package contract: headers resolve from the
 * install prefix, the runtime links with the installed linker script and the
 * startup objects survive the link. It uses the public umbrella header and
 * nothing from the repository tree. */
#include <saturn/saturn.h>
#include <saturn/int_math.h>
#include <saturn/fx_compositor.h>
#include <saturn/profile.h>
#include <saturn/water.h>
#include <saturn/water_volume.h>

#if !defined(SATURN_VERSION_STRING) || !defined(SATURN_VERSION_AT_LEAST)
#error "the installed package must provide <saturn/version.h>"
#endif

int main(void) {
    sat_pad_state_t pad;
    sat_profile_t profile;
    sat_profile_counter_t counters[1];
    sat_profile_span_t spans[1];
    if (!sat_profile_init(&profile, counters, 1, spans, 1) ||
        !sat_profile_enter(&profile, 0, 1) ||
        !sat_profile_leave(&profile, 0, 3) ||
        !sat_profile_complete(&profile) || counters[0].exclusive != 2) {
        for (;;) {}
    }
    volatile int32_t negative_fraction = -4097;
    if (sat_floor_shift_i32(negative_fraction, 12u) != -2 ||
        sat_floor_shift_i64((int64_t)negative_fraction, 12u) != -2) {
        for (;;) {}
    }

    const sat_quad3_t face = {{{0, 0, 0}, {SAT_FX16_ONE, 0, 0},
        {SAT_FX16_ONE, SAT_FX16_ONE, 0}, {0, SAT_FX16_ONE, 0}}};
    sat_water_piece_t pieces[SAT_WATER_SPLIT_PIECES_MAX];
    uint8_t count;
    const sat_water_volume_t water = {.max_x = SAT_FX16_ONE, .max_z = SAT_FX16_ONE,
        .surface = SAT_FX16_ONE / 2};
    sat_water_sample_t sample;
    if (sat_water_split_quad(&face, water.surface, pieces, &count) != SAT_OK ||
        count != 2 || sat_water_volume_sample(&water, face.v, SAT_FX16_ONE, &sample) != SAT_OK ||
        sample.fraction != SAT_FX16_ONE / 2) {
        for (;;) {}
    }
    static sat_fx_compositor_t fx;
    if (sat_fx_compositor_reserve(&fx) != SAT_ERR_NOT_INITIALIZED) {
        for (;;) {}
    }

    if (sat_app_init_default() != SAT_OK) {
        for (;;) {
        }
    }
    volatile uint8_t cache_control = sat_sh2_cache_control();
    (void)cache_control;
    for (;;) {
        if (sat_app_frame_begin(SAT_COLOR_BLUE, SAT_COLOR_BLACK, &pad) != SAT_OK) {
            continue;
        }
        (void)sat_app_frame_end();
    }
}
