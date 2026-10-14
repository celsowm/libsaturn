#include "saturn/tate.h"

/* Pure arithmetic, no hardware: see saturn/tate.h. */

namespace {

constexpr uint16_t kMaxDim = 2048u;

/* Rotated, unscaled space: the logical edge point (lx, ly) as it sits in the
 * rotated field before scaling and placement. */
inline void rotate_edge(uint8_t rotation, int32_t lw, int32_t lh, int32_t lx, int32_t ly,
                        int32_t* rx, int32_t* ry) {
    switch (rotation) {
    case SAT_TATE_ROT_CW:  *rx = lh - ly; *ry = lx; break;
    case SAT_TATE_ROT_CCW: *rx = ly; *ry = lw - lx; break;
    default:               *rx = lx; *ry = ly; break;
    }
}

inline void unrotate_edge(uint8_t rotation, int32_t lw, int32_t lh, int32_t rx, int32_t ry,
                          int32_t* lx, int32_t* ly) {
    switch (rotation) {
    case SAT_TATE_ROT_CW:  *lx = ry; *ly = lh - rx; break;
    case SAT_TATE_ROT_CCW: *lx = lw - ry; *ly = rx; break;
    default:               *lx = rx; *ly = ry; break;
    }
}

/* floor(v * num / den) for any v (floor division, not truncation). */
inline int32_t scale_floor(int32_t v, int32_t num, int32_t den) {
    const int64_t p = static_cast<int64_t>(v) * num;
    int64_t q = p / den;
    if (p % den != 0 && ((p < 0) != (den < 0))) --q;
    return static_cast<int32_t>(q);
}

inline bool valid(const sat_tate_layout_t* l) {
    return l != nullptr && l->scale_den != 0u && l->logical_w != 0u && l->logical_h != 0u;
}

}  // namespace

extern "C" sat_result_t sat_tate_layout_init(sat_tate_layout_t* layout, uint16_t screen_w, uint16_t screen_h,
                                             uint16_t logical_w, uint16_t logical_h, uint8_t rotation) {
    if (layout == nullptr) return SAT_ERR_INVALID_ARG;
    *layout = sat_tate_layout_t{};
    if (screen_w == 0u || screen_h == 0u || logical_w == 0u || logical_h == 0u ||
        screen_w > kMaxDim || screen_h > kMaxDim || logical_w > kMaxDim || logical_h > kMaxDim ||
        rotation > SAT_TATE_ROT_CCW) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t rw = rotation == SAT_TATE_UPRIGHT ? logical_w : logical_h;
    const uint32_t rh = rotation == SAT_TATE_UPRIGHT ? logical_h : logical_w;

    uint32_t num = 1u, den = 1u;
    if (rw > screen_w || rh > screen_h) {
        /* The largest ratio that fits, limited by whichever dimension is tighter. */
        if (static_cast<uint32_t>(screen_w) * rh <= static_cast<uint32_t>(screen_h) * rw) {
            num = screen_w; den = rw;
        } else {
            num = screen_h; den = rh;
        }
    }
    const uint32_t fw = rw * num / den;
    const uint32_t fh = rh * num / den;
    if (fw == 0u || fh == 0u) return SAT_ERR_INVALID_ARG;

    layout->logical_w = logical_w;
    layout->logical_h = logical_h;
    layout->screen_w = screen_w;
    layout->screen_h = screen_h;
    layout->scale_num = static_cast<uint16_t>(num);
    layout->scale_den = static_cast<uint16_t>(den);
    layout->rotation = rotation;
    layout->exact = num == den ? 1u : 0u;
    layout->field.x = static_cast<int16_t>((screen_w - fw) / 2u);
    layout->field.y = static_cast<int16_t>((screen_h - fh) / 2u);
    layout->field.width = static_cast<uint16_t>(fw);
    layout->field.height = static_cast<uint16_t>(fh);

    const sat_rect_t& f = layout->field;
    if (f.width < screen_w) {
        layout->panel[layout->panel_count++] = sat_rect_t{0, 0, static_cast<uint16_t>(f.x), screen_h};
        layout->panel[layout->panel_count++] = sat_rect_t{
            static_cast<int16_t>(f.x + f.width), 0,
            static_cast<uint16_t>(screen_w - f.x - f.width), screen_h};
    } else if (f.height < screen_h) {
        layout->panel[layout->panel_count++] = sat_rect_t{0, 0, screen_w, static_cast<uint16_t>(f.y)};
        layout->panel[layout->panel_count++] = sat_rect_t{
            0, static_cast<int16_t>(f.y + f.height), screen_w,
            static_cast<uint16_t>(screen_h - f.y - f.height)};
    }
    /* Drop empty strips (an odd leftover can leave one side with nothing). */
    uint8_t kept = 0u;
    for (uint8_t i = 0; i < layout->panel_count; ++i) {
        if (layout->panel[i].width != 0u && layout->panel[i].height != 0u) layout->panel[kept++] = layout->panel[i];
    }
    for (uint8_t i = kept; i < 2u; ++i) layout->panel[i] = sat_rect_t{};
    layout->panel_count = kept;
    return SAT_OK;
}

extern "C" sat_result_t sat_tate_edge(const sat_tate_layout_t* layout, int32_t lx, int32_t ly,
                                      int32_t* out_sx, int32_t* out_sy) {
    if (!valid(layout) || out_sx == nullptr || out_sy == nullptr) return SAT_ERR_INVALID_ARG;
    int32_t rx = 0, ry = 0;
    rotate_edge(layout->rotation, layout->logical_w, layout->logical_h, lx, ly, &rx, &ry);
    *out_sx = layout->field.x + scale_floor(rx, layout->scale_num, layout->scale_den);
    *out_sy = layout->field.y + scale_floor(ry, layout->scale_num, layout->scale_den);
    return SAT_OK;
}

extern "C" sat_result_t sat_tate_rect(const sat_tate_layout_t* layout, const sat_rect_t* logical,
                                      sat_rect_t* out_screen) {
    if (!valid(layout) || logical == nullptr || out_screen == nullptr) return SAT_ERR_INVALID_ARG;
    int32_t ax = 0, ay = 0, bx = 0, by = 0;
    SAT_TRY(sat_tate_edge(layout, logical->x, logical->y, &ax, &ay));
    SAT_TRY(sat_tate_edge(layout, logical->x + logical->width, logical->y + logical->height, &bx, &by));
    const int32_t x0 = ax < bx ? ax : bx, x1 = ax < bx ? bx : ax;
    const int32_t y0 = ay < by ? ay : by, y1 = ay < by ? by : ay;
    if (x0 < INT16_MIN || x1 > INT16_MAX || y0 < INT16_MIN || y1 > INT16_MAX) return SAT_ERR_INVALID_ARG;
    out_screen->x = static_cast<int16_t>(x0);
    out_screen->y = static_cast<int16_t>(y0);
    out_screen->width = static_cast<uint16_t>(x1 - x0);
    out_screen->height = static_cast<uint16_t>(y1 - y0);
    return SAT_OK;
}

extern "C" sat_result_t sat_tate_unmap_edge(const sat_tate_layout_t* layout, int32_t sx, int32_t sy,
                                            int32_t* out_lx, int32_t* out_ly) {
    if (!valid(layout) || out_lx == nullptr || out_ly == nullptr) return SAT_ERR_INVALID_ARG;
    const int32_t rx = scale_floor(sx - layout->field.x, layout->scale_den, layout->scale_num);
    const int32_t ry = scale_floor(sy - layout->field.y, layout->scale_den, layout->scale_num);
    unrotate_edge(layout->rotation, layout->logical_w, layout->logical_h, rx, ry, out_lx, out_ly);
    return SAT_OK;
}

extern "C" sat_result_t sat_tate_pixel(const sat_tate_layout_t* layout, int32_t lx, int32_t ly,
                                       int32_t* out_sx, int32_t* out_sy) {
    if (!valid(layout) || out_sx == nullptr || out_sy == nullptr) return SAT_ERR_INVALID_ARG;
    const sat_rect_t unit = {static_cast<int16_t>(lx), static_cast<int16_t>(ly), 1u, 1u};
    sat_rect_t r{};
    SAT_TRY(sat_tate_rect(layout, &unit, &r));
    *out_sx = r.x;
    *out_sy = r.y;
    return SAT_OK;
}

extern "C" void sat_tate_size(const sat_tate_layout_t* layout, uint16_t w, uint16_t h,
                              uint16_t* out_w, uint16_t* out_h) {
    const bool swap = layout != nullptr && layout->rotation != SAT_TATE_UPRIGHT;
    if (out_w != nullptr) *out_w = swap ? h : w;
    if (out_h != nullptr) *out_h = swap ? w : h;
}
