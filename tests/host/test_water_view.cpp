#include <cassert>
#include "saturn/water_view.h"

namespace {

constexpr sat_fx16_t one = SAT_FX16_ONE;

struct Setup {
    sat_water_volume_t volume{};
    sat_water_regions_t regions;
    sat_water_view_config_t config{};
    sat_water_view_t view;
    Setup() {
        volume.min_x = -8 * one; volume.max_x = 8 * one;
        volume.min_z = -8 * one; volume.max_z = 8 * one;
        volume.bottom = -8 * one; volume.surface = 0; volume.drag = one / 5;
        assert(sat_water_regions_init(&regions, &volume, 1) == SAT_OK);
        config.regions = &regions;
        config.hysteresis = one / 8;
        config.bank = SAT_VDP2_COLOR_OFFSET_B;
        config.layers = SAT_VDP2_LAYER_SPRITE | SAT_VDP2_LAYER_BACK;
        config.tint = {-120, -40, 0};
        config.fade_frames = 4;
        assert(sat_water_view_init(&view, &config) == SAT_OK);
    }
};

void validation() {
    Setup s;
    sat_water_view_t v;
    auto c = s.config;
    assert(sat_water_view_init(nullptr, &c) == SAT_ERR_INVALID_ARG);
    c.layers = 0; assert(sat_water_view_init(&v, &c) == SAT_ERR_INVALID_ARG);
    c = s.config; c.fade_frames = 0; assert(sat_water_view_init(&v, &c) == SAT_ERR_INVALID_ARG);
    c = s.config; c.hysteresis = -1; assert(sat_water_view_init(&v, &c) == SAT_ERR_INVALID_ARG);
    c = s.config; c.tint.r = -300; assert(sat_water_view_init(&v, &c) == SAT_ERR_INVALID_ARG);
    c = s.config; c.regions = nullptr; assert(sat_water_view_init(&v, &c) == SAT_ERR_INVALID_ARG);
    c = s.config; c.bank = static_cast<sat_vdp2_color_offset_bank_t>(2);
    assert(sat_water_view_init(&v, &c) == SAT_ERR_INVALID_ARG);
}

void dry_camera_is_quiet() {
    Setup s;
    sat_water_view_state_t st;
    const sat_vec3_t eye = {0, 3 * one, 0};
    for (int i = 0; i < 10; ++i) {
        assert(sat_water_view_update(&s.view, &eye, &st) == SAT_OK);
        assert(!st.underwater && st.region == SAT_WATER_REGION_NONE && st.fade == 0);
        assert(!st.set_offset && !st.release && st.offset.r == 0);
    }
}

void dive_fades_in_and_out_and_releases_once() {
    Setup s;
    sat_water_view_state_t st;
    sat_vec3_t eye = {0, -2 * one, 0};
    int last = 0;
    for (int frame = 1; frame <= 6; ++frame) {
        assert(sat_water_view_update(&s.view, &eye, &st) == SAT_OK);
        assert(st.underwater && st.region == 0 && st.set_offset && !st.release);
        const int want = frame < 4 ? frame : 4;
        assert(st.fade == want);
        assert(st.offset.r == -120 * want / 4 && st.offset.g == -40 * want / 4 && st.offset.b == 0);
        assert(st.offset.r <= last || frame == 1);
        last = st.offset.r;
    }
    assert(st.offset.r == -120);  // full tint, reached exactly
    // Surfacing inside the hysteresis band keeps the view underwater.
    eye.y = one / 16;
    assert(sat_water_view_update(&s.view, &eye, &st) == SAT_OK && st.underwater);
    eye.y = one;
    int releases = 0, sets = 0;
    for (int frame = 0; frame < 8; ++frame) {
        assert(sat_water_view_update(&s.view, &eye, &st) == SAT_OK);
        assert(!st.underwater);
        sets += st.set_offset;
        releases += st.release;
        if (frame == 0) assert(st.fade == 3 && st.set_offset);
    }
    assert(releases == 1 && sets == 3 && st.fade == 0 && !st.set_offset && st.offset.r == 0);
}

void a_quick_bob_does_not_skip_the_release() {
    Setup s;
    sat_water_view_state_t st;
    sat_vec3_t eye = {0, -2 * one, 0};
    assert(sat_water_view_update(&s.view, &eye, &st) == SAT_OK && st.fade == 1);
    eye.y = 2 * one;
    assert(sat_water_view_update(&s.view, &eye, &st) == SAT_OK);
    assert(st.fade == 0 && st.release && !st.set_offset);
}

void passes_mirror_under_water() {
    const auto above = sat_water_view_passes(0);
    const auto under = sat_water_view_passes(1);
    assert(above.below < above.water && above.water < above.above);
    assert(under.above < under.water && under.water < under.below);
}

}

int main() {
    validation();
    dry_camera_is_quiet();
    dive_fades_in_and_out_and_releases_once();
    a_quick_bob_does_not_skip_the_release();
    passes_mirror_under_water();
}
