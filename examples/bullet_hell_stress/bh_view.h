#ifndef BH_VIEW_H
#define BH_VIEW_H

/* Logical playfield -> screen, for sprites that were rotated offline
 * (tools/shmup_assets.py pack --orientation). The 224 x 448 field is turned a quarter and
 * placed by saturn/tate.h; because that layout is 1:1 on the hi-res screen, a point maps with
 * two adds and no scaling, so a thousand bullets cost almost nothing here.
 *
 * Logical points are pixel edges, the convention of sat_tate_edge, and the pivot of an atlas
 * frame is rotated with the same convention, so a sprite placed at the mapped point lands on
 * exactly the pixels it would have covered upright. test_bh_sim checks the inline map
 * against sat_tate_edge. */

#include <stdint.h>

#include "saturn/tate.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bh_view {
    sat_tate_layout_t layout;
    int32_t c0, d0;            /* sx = c0 + sxx * lx + sxy * ly ; sy = d0 + syx * lx + syy * ly */
    int8_t sxx, sxy, syx, syy;
} bh_view_t;

/* SAT_ERR_UNSUPPORTED when the field does not fit 1:1 (a scaled layout needs a scaled batch). */
static inline sat_result_t bh_view_init(bh_view_t* v, uint16_t screen_w, uint16_t screen_h,
                                        uint16_t logical_w, uint16_t logical_h, uint8_t rotation) {
    const sat_result_t r = sat_tate_layout_init(&v->layout, screen_w, screen_h, logical_w, logical_h, rotation);
    if (r != SAT_OK) return r;
    if (!v->layout.exact) return SAT_ERR_UNSUPPORTED;
    const int32_t fx = v->layout.field.x, fy = v->layout.field.y;
    switch (rotation) {
    case SAT_TATE_ROT_CW:
        v->c0 = fx + logical_h; v->d0 = fy; v->sxx = 0; v->sxy = -1; v->syx = 1; v->syy = 0;
        break;
    case SAT_TATE_ROT_CCW:
        v->c0 = fx; v->d0 = fy + logical_w; v->sxx = 0; v->sxy = 1; v->syx = -1; v->syy = 0;
        break;
    default:
        v->c0 = fx; v->d0 = fy; v->sxx = 1; v->sxy = 0; v->syx = 0; v->syy = 1;
        break;
    }
    return SAT_OK;
}

static inline void bh_view_map(const bh_view_t* v, int32_t lx, int32_t ly, int16_t* sx, int16_t* sy) {
    *sx = (int16_t)(v->c0 + v->sxx * lx + v->sxy * ly);
    *sy = (int16_t)(v->d0 + v->syx * lx + v->syy * ly);
}

#ifdef __cplusplus
}
#endif
#endif
