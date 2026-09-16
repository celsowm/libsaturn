#ifndef BG_H
#define BG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SAT_INDEXED8_ASSET_T_DEFINED
#define SAT_INDEXED8_ASSET_T_DEFINED
typedef struct sat_indexed8_asset {
    const uint8_t* pixels;
    const uint16_t* palette;
    uint16_t width;
    uint16_t height;
    uint16_t palette_index;
    uint32_t pixel_count;
    uint32_t palette_count;
} sat_indexed8_asset_t;
#endif

extern const uint8_t bg_pixels[71680];
extern const uint16_t bg_palette[256];
extern const sat_indexed8_asset_t bg_asset;

#ifdef __cplusplus
}
#endif

#endif /* BG_H */
