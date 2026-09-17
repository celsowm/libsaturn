#ifndef SATURN_COLOR_H
#define SATURN_COLOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Game-facing color and pixel formats                                */
/* ------------------------------------------------------------------ */
typedef struct sat_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} sat_color_t;

typedef enum sat_pixel_format {
    SAT_PIXEL_INDEX8 = 0,
    SAT_PIXEL_RGB555,
    SAT_PIXEL_ARGB1555,
    SAT_PIXEL_RGB565,
    SAT_PIXEL_RGBA8888
} sat_pixel_format_t;

static inline sat_color_t sat_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    sat_color_t color = {r, g, b, a};
    return color;
}

/* ------------------------------------------------------------------ */
/* Saturn-native color construction                                   */
/* ------------------------------------------------------------------ */
/* Saturn color words are BGR555: RED occupies bits 0-4, green bits 5-9 and
 * BLUE bits 10-14 -- the reverse of the channel order the name "RGB555"
 * suggests. Writing the shift by hand is therefore easy to get backwards, and
 * the result is a plausible-looking picture in the wrong colors (a yellow
 * Pac-Man comes out cyan), which is why this macro exists.
 *
 * Bit 15 is the RGB code: it marks the value as a direct color rather than a
 * color-bank index, and must be set on every color handed to a VDP1 polygon,
 * line or erase command. Cross-check against the named constants below:
 * SAT_RGB555(31, 0, 0) is SAT_COLOR_RED with bit 15 set.
 */
#define SAT_RGB555(r, g, b) ((uint16_t)(0x8000u | \
    (((uint16_t)(b) & 0x1Fu) << 10u) | \
    (((uint16_t)(g) & 0x1Fu) << 5u) | \
    ((uint16_t)(r) & 0x1Fu)))

/* Same channel order, without the RGB code bit -- for CRAM palette entries,
 * which are indexed rather than direct. */
#define SAT_BGR555(r, g, b) ((uint16_t)( \
    (((uint16_t)(b) & 0x1Fu) << 10u) | \
    (((uint16_t)(g) & 0x1Fu) << 5u) | \
    ((uint16_t)(r) & 0x1Fu)))

/* ------------------------------------------------------------------ */
/* Named Saturn BGR555 colors                                         */
/* ------------------------------------------------------------------ */
#define SAT_COLOR_BLACK   ((uint16_t)0x0000)
#define SAT_COLOR_BLUE    ((uint16_t)0x7C00)
#define SAT_COLOR_GREEN   ((uint16_t)0x03E0)
#define SAT_COLOR_RED     ((uint16_t)0x001F)
#define SAT_COLOR_YELLOW  ((uint16_t)0x03FF)
#define SAT_COLOR_MAGENTA ((uint16_t)0x7C1F)
#define SAT_COLOR_CYAN    ((uint16_t)0x7FE0)
#define SAT_COLOR_WHITE   ((uint16_t)0x7FFF)
#define SAT_COLOR_ORANGE  ((uint16_t)0x021F)
#define SAT_COLOR_VIOLET  ((uint16_t)0x7C10)
#define SAT_COLOR_GRAY    ((uint16_t)0x2210)
#define SAT_COLOR_TEAL    ((uint16_t)0x4200)
#define SAT_COLOR_OLIVE   ((uint16_t)0x4210)
#define SAT_COLOR_BROWN   ((uint16_t)0x4016)

#ifdef __cplusplus
}
#endif

#endif /* SATURN_COLOR_H */
