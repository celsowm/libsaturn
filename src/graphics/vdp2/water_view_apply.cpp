#include "saturn/water_view.h"

extern "C" sat_result_t sat_water_view_apply(const sat_water_view_t* view,
    const sat_water_view_state_t* state) {
    if (!view || !state) return SAT_ERR_INVALID_ARG;
    if (state->set_offset) {
        sat_result_t r = sat_vdp2_color_offset_set(view->config.bank, &state->offset);
        if (r != SAT_OK) return r;
        return sat_vdp2_color_offset_enable(view->config.layers, view->config.bank);
    }
    if (state->release) return sat_vdp2_color_offset_disable(view->config.layers);
    return SAT_OK;
}
