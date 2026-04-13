/* demo.c - Shared boilerplate for libsaturn examples */
#include "examples/common/demo.h"

sat_result_t demo_init_default(void) {
    sat_video_config_t cfg = {320, 224, 1, 0};
    return sat_init(&cfg);
}

sat_result_t demo_frame_begin(uint16_t backdrop_rgb555, uint16_t clear_rgb555,
                              sat_pad_state_t* out_pad) {
    sat_result_t st = SAT_OK;

    st = sat_wait_vblank();
    if (st != SAT_OK) return st;

    st = sat_vdp2_back_color_set(backdrop_rgb555);
    if (st != SAT_OK) return st;

    st = sat_set_clear_color(clear_rgb555);
    if (st != SAT_OK) return st;

    st = sat_begin_frame();
    if (st != SAT_OK) return st;

    if (out_pad != NULL) {
        st = sat_pad_poll(out_pad);
        if (st != SAT_OK) return st;
    }

    return SAT_OK;
}

sat_result_t demo_frame_end(void) {
    return sat_end_frame();
}
