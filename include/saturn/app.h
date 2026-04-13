#ifndef SATURN_APP_H
#define SATURN_APP_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/input.h"
#include "saturn/video.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Example-friendly app helpers                                        */
/* ------------------------------------------------------------------ */
sat_result_t sat_app_init_default(void);
sat_result_t sat_app_frame_begin(
    uint16_t backdrop_rgb555,
    uint16_t clear_rgb555,
    sat_pad_state_t* out_pad
);
sat_result_t sat_app_frame_end(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_APP_H */
