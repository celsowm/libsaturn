#ifndef SATURN_FADE3D_H
#define SATURN_FADE3D_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Quantized distance fade policy. The policy itself knows nothing about VDP1
 * or VDP2: it only maps camera/view depth to a discrete level. */
#define SAT_FADE3D_CULL_AFTER_END 0x01u

typedef struct sat_fade3d {
    sat_fx16_t start;
    sat_fx16_t end;
    uint8_t levels;
    uint8_t flags;
    uint16_t reserved;
} sat_fade3d_t;

typedef struct sat_fade3d_result {
    uint8_t level;
    uint8_t culled;
    uint16_t reserved;
} sat_fade3d_result_t;

/* depth <= start -> level 0.
 * start < depth < end -> one of levels quantized buckets.
 * depth >= end -> final level, or culled when SAT_FADE3D_CULL_AFTER_END is set.
 *
 * `levels` is generic and may be larger than the Saturn VDP2's eight sprite
 * color-calculation slots. A Saturn-specific caller chooses how levels map to
 * hardware slots. */
sat_result_t sat_fade3d_eval(
    const sat_fade3d_t* fade,
    sat_fx16_t view_depth,
    sat_fade3d_result_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_FADE3D_H */
