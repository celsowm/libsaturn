#include "saturn/app.h"

extern "C" sat_result_t sat_app_init_default(void) {
    sat_video_config_t cfg = {320, 224, 1, 0};
    return sat_init(&cfg);
}

extern "C" sat_result_t sat_app_frame_begin(
    uint16_t backdrop_rgb555,
    uint16_t clear_rgb555,
    sat_pad_state_t* out_pad
) {
    sat_result_t st = sat_wait_vblank();
    if (st != SAT_OK) {
        return st;
    }

    st = sat_vdp2_back_color_set(backdrop_rgb555);
    if (st != SAT_OK) {
        return st;
    }

    st = sat_set_clear_color(clear_rgb555);
    if (st != SAT_OK) {
        return st;
    }

    // VDP1 faz erase com cor transparente (end code), permitindo que o
    // backdrop do VDP2 apareça nas áreas sem sprites.
    st = sat_begin_frame();
    if (st != SAT_OK) {
        return st;
    }

    if (out_pad != nullptr) {
        st = sat_pad_poll(out_pad);
        if (st != SAT_OK) {
            return st;
        }
    }

    return SAT_OK;
}

extern "C" sat_result_t sat_app_frame_end(void) {
    return sat_end_frame();
}
