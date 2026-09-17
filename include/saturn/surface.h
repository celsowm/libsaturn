#ifndef SATURN_SURFACE_H
#define SATURN_SURFACE_H

#include <stdint.h>
#include "saturn/core.h"
#include "saturn/color.h"
#include "saturn/geometry2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pixel packing used by this API:
 * - INDEX8 stores one palette index per byte; palette_rgb555 uses the same
 *   Saturn RGB555 words produced by LibSaturn's asset tools.
 * - RGB555 uses Saturn channel order (R bits 0-4, G 5-9, B 10-14) and writes
 *   bit 15 as the opaque/RGB-code bit.
 * - ARGB1555 uses the same channels and bit 15 as 1-bit alpha.
 * - RGB565 uses R bits 0-4, G 5-10, B 11-15.
 * - RGBA8888 stores byte-addressed R, G, B, A in that order. */
typedef struct sat_surface {
    void* pixels;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    sat_pixel_format_t format;
    const uint16_t* palette_rgb555;
    uint16_t palette_count;
} sat_surface_t;

uint8_t sat_pixel_format_bytes_per_pixel(sat_pixel_format_t format);

sat_result_t sat_surface_init(
    sat_surface_t* surface,
    void* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t pitch,
    sat_pixel_format_t format,
    const uint16_t* palette_rgb555,
    uint16_t palette_count);

sat_result_t sat_surface_subview(
    const sat_surface_t* source,
    const sat_rect_t* rect,
    sat_surface_t* out_surface);

/* rect == NULL fills the entire surface. Rectangles are clipped to bounds. */
sat_result_t sat_surface_fill(
    sat_surface_t* surface,
    const sat_rect_t* rect,
    sat_color_t color);

/* src_rect == NULL selects the whole source. Source and destination are
 * clipped. Same-format overlapping blits are memmove-safe when pitches match;
 * unsupported overlapping layouts return SAT_ERR_UNSUPPORTED. */
sat_result_t sat_surface_blit(
    sat_surface_t* destination,
    sat_point_t destination_position,
    const sat_surface_t* source,
    const sat_rect_t* src_rect);

/* Nearest-neighbor scaling. Overlapping source/destination storage is rejected
 * because no temporary allocation is performed. */
sat_result_t sat_surface_blit_scaled(
    sat_surface_t* destination,
    const sat_rect_t* destination_rect,
    const sat_surface_t* source,
    const sat_rect_t* src_rect);

/* Converts the complete source into an equally-sized destination. */
sat_result_t sat_surface_convert(sat_surface_t* destination, const sat_surface_t* source);

sat_result_t sat_surface_get_pixel(
    const sat_surface_t* surface,
    uint16_t x,
    uint16_t y,
    sat_color_t* out_color);

sat_result_t sat_surface_set_pixel(
    sat_surface_t* surface,
    uint16_t x,
    uint16_t y,
    sat_color_t color);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SURFACE_H */
