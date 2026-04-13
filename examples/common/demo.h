/* demo.h - Shared boilerplate for libsaturn examples */
#ifndef EXAMPLES_COMMON_DEMO_H
#define EXAMPLES_COMMON_DEMO_H

#include "saturn/saturn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize with default 320x224 NTSC config.
 * Returns SAT_OK on success, or error code from sat_init(). */
sat_result_t demo_init_default(void);

/* Begin a new frame:
 *  1. Wait for VBlank
 *  2. Set backdrop color
 *  3. Set clear color
 *  4. Call sat_begin_frame()
 *  5. Poll pad state into out_pad (may be NULL to skip)
 */
sat_result_t demo_frame_begin(uint16_t backdrop_rgb555, uint16_t clear_rgb555,
                              sat_pad_state_t* out_pad);

/* End the current frame: call sat_end_frame(). */
sat_result_t demo_frame_end(void);

#ifdef __cplusplus
}
#endif

#endif /* EXAMPLES_COMMON_DEMO_H */
