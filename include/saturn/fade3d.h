#ifndef SATURN_FADE3D_H
#define SATURN_FADE3D_H

#include <stdint.h>

#include "saturn/core.h"
/* The slot binding below returns SAT_INDEXED_SOLID_OPAQUE, which render3d.h
 * defines. sat_fade3d_eval itself stays free of any hardware notion. */
#include "saturn/render3d.h"

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

/* ---- Saturn binding -------------------------------------------------------
 * sat_fade3d_eval answers "which level", which still leaves every caller to
 * re-derive the same two things: how a level maps to one of the VDP2's eight
 * sprite color-calculation slots, and how to stop an object flickering between
 * two slots while the camera hovers on a boundary. Both belong here. */

/* Beyond the policy's end, when SAT_FADE3D_CULL_AFTER_END is set: do not draw.
 * "Opaque" reuses SAT_INDEXED_SOLID_OPAQUE (255) from render3d.h rather than
 * inventing a second name for the same hardware state. */
#define SAT_FADE3D_SLOT_CULLED 254u

typedef struct sat_fade3d_slots {
    sat_fade3d_t policy;
    /* Depth band, in view units, that a transition must be crossed by before
     * the slot changes. 0 disables hysteresis entirely. */
    sat_fx16_t hysteresis;
    uint8_t base_slot;   /* level 0 maps here */
    uint8_t slot_stride; /* level n maps to base_slot + n*slot_stride */
    uint8_t opaque_before_start; /* 1: depth <= start is opaque, not a slot */
    uint8_t reserved;
} sat_fade3d_slots_t;

/* Maps view depth straight to a slot ready for sat_scene3d_faces_submit_* or
 * sat_draw_sprite_*_color_calc: 0..7, SAT_INDEXED_SOLID_OPAQUE, or
 * SAT_FADE3D_SLOT_CULLED.
 *
 * inout_state is one caller-owned byte per object holding the slot last
 * returned for it, and is what makes hysteresis work; pass NULL for stateless
 * evaluation. Initialize it to SAT_INDEXED_SOLID_OPAQUE. Consistent with the
 * rest of the library, nothing is allocated and the caller owns the storage. */
sat_result_t sat_fade3d_slot(
    const sat_fade3d_slots_t* config,
    sat_fx16_t view_depth,
    uint8_t* inout_state,
    uint8_t* out_slot
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_FADE3D_H */
