#ifndef SATURN_FONT_H
#define SATURN_FONT_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Bitmap font packing helper                                          */
/* ------------------------------------------------------------------ */
sat_result_t sat_font_pack_8x8_glyph_indexed8(
    uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t dst_x,
    uint16_t dst_y,
    const uint8_t* glyph_rows,
    uint8_t scale
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_FONT_H */
