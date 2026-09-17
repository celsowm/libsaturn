#ifndef SATURN_GEOMETRY2D_H
#define SATURN_GEOMETRY2D_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sat_point {
    int16_t x;
    int16_t y;
} sat_point_t;

typedef struct sat_rect {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} sat_rect_t;

static inline int sat_rect_empty(const sat_rect_t* rect) {
    return rect == 0 || rect->width == 0u || rect->height == 0u;
}

static inline int sat_rect_contains_point(const sat_rect_t* rect, sat_point_t point) {
    if (sat_rect_empty(rect)) {
        return 0;
    }
    const int32_t right = (int32_t)rect->x + (int32_t)rect->width;
    const int32_t bottom = (int32_t)rect->y + (int32_t)rect->height;
    return (int32_t)point.x >= (int32_t)rect->x &&
           (int32_t)point.y >= (int32_t)rect->y &&
           (int32_t)point.x < right &&
           (int32_t)point.y < bottom;
}

static inline int sat_rect_intersect(const sat_rect_t* a, const sat_rect_t* b, sat_rect_t* out) {
    if (out != 0) {
        out->x = 0;
        out->y = 0;
        out->width = 0u;
        out->height = 0u;
    }
    if (sat_rect_empty(a) || sat_rect_empty(b)) {
        return 0;
    }

    const int32_t a_right = (int32_t)a->x + (int32_t)a->width;
    const int32_t a_bottom = (int32_t)a->y + (int32_t)a->height;
    const int32_t b_right = (int32_t)b->x + (int32_t)b->width;
    const int32_t b_bottom = (int32_t)b->y + (int32_t)b->height;

    const int32_t left = a->x > b->x ? (int32_t)a->x : (int32_t)b->x;
    const int32_t top = a->y > b->y ? (int32_t)a->y : (int32_t)b->y;
    const int32_t right = a_right < b_right ? a_right : b_right;
    const int32_t bottom = a_bottom < b_bottom ? a_bottom : b_bottom;

    if (right <= left || bottom <= top) {
        return 0;
    }

    if (out != 0) {
        out->x = (int16_t)left;
        out->y = (int16_t)top;
        out->width = (uint16_t)(right - left);
        out->height = (uint16_t)(bottom - top);
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* SATURN_GEOMETRY2D_H */
