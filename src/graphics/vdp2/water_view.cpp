#include "saturn/water_view.h"

namespace {

int16_t scale(int16_t v, uint8_t fade, uint8_t frames) {
    return static_cast<int16_t>(static_cast<int32_t>(v) * fade / frames);
}

}

extern "C" sat_result_t sat_water_view_init(sat_water_view_t* view, const sat_water_view_config_t* c) {
    if (!view || !c || !c->regions || c->hysteresis < 0 || c->layers == 0 || c->fade_frames == 0 ||
        (c->bank != SAT_VDP2_COLOR_OFFSET_A && c->bank != SAT_VDP2_COLOR_OFFSET_B) ||
        c->tint.r < -256 || c->tint.r > 255 || c->tint.g < -256 || c->tint.g > 255 ||
        c->tint.b < -256 || c->tint.b > 255)
        return SAT_ERR_INVALID_ARG;
    *view = {*c, SAT_WATER_REGION_NONE, 0};
    return SAT_OK;
}

extern "C" sat_result_t sat_water_view_update(sat_water_view_t* view, const sat_vec3_t* eye,
    sat_water_view_state_t* out) {
    if (!view || !eye || !out) return SAT_ERR_INVALID_ARG;
    uint16_t region = SAT_WATER_REGION_NONE;
    const sat_result_t r = sat_water_regions_immersion(view->config.regions, eye,
        view->config.hysteresis, view->region, &region);
    if (r != SAT_OK) return r;
    const uint8_t before = view->fade;
    view->region = region;
    if (region != SAT_WATER_REGION_NONE) {
        if (view->fade < view->config.fade_frames) ++view->fade;
    } else if (view->fade > 0) {
        --view->fade;
    }
    out->region = region;
    out->underwater = region != SAT_WATER_REGION_NONE;
    out->fade = view->fade;
    out->offset = {scale(view->config.tint.r, view->fade, view->config.fade_frames),
                   scale(view->config.tint.g, view->fade, view->config.fade_frames),
                   scale(view->config.tint.b, view->fade, view->config.fade_frames)};
    out->set_offset = view->fade > 0;
    out->release = before > 0 && view->fade == 0;
    return SAT_OK;
}

extern "C" sat_water_view_passes_t sat_water_view_passes(uint8_t underwater) {
    return underwater ? sat_water_view_passes_t{2, 1, 0} : sat_water_view_passes_t{0, 1, 2};
}
