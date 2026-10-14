#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <utility>
#include <vector>

#include "saturn/tate.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

namespace {

uint32_t g_rng = 0xC0FFEEu;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

bool same(const sat_rect_t& a, int x, int y, int w, int h) {
    return a.x == x && a.y == y && a.width == w && a.height == h;
}

sat_tate_layout_t make(uint16_t sw, uint16_t sh, uint8_t rot, uint16_t lw = 224, uint16_t lh = 448) {
    sat_tate_layout_t l{};
    OK(sat_tate_layout_init(&l, sw, sh, lw, lh, rot) == SAT_OK);
    return l;
}

}  // namespace

int main() {
    /* Hi-res 640 x 224: the quarter-turned 224 x 448 field is 448 x 224 at 1:1, 96 px panels each side. */
    for (uint8_t rot : {SAT_TATE_ROT_CW, SAT_TATE_ROT_CCW}) {
        const sat_tate_layout_t l = make(640u, 224u, rot);
        OK(l.exact == 1u && l.scale_num == l.scale_den);
        OK(same(l.field, 96, 0, 448, 224));
        OK(l.panel_count == 2u && same(l.panel[0], 0, 0, 96, 224) && same(l.panel[1], 544, 0, 96, 224));
    }

    /* Normal 320 x 224: scaled to 320 x 160, centred, 32 lines above and below. */
    {
        const sat_tate_layout_t l = make(320u, 224u, SAT_TATE_ROT_CW);
        OK(l.exact == 0u && l.scale_num == 320u && l.scale_den == 448u);
        OK(same(l.field, 0, 32, 320, 160));
        OK(l.panel_count == 2u && same(l.panel[0], 0, 0, 320, 32) && same(l.panel[1], 0, 192, 320, 32));
    }

    /* Upright on a portrait-capable screen is the identity; on a landscape one it shrinks to the height. */
    {
        const sat_tate_layout_t l = make(224u, 448u, SAT_TATE_UPRIGHT);
        OK(l.exact == 1u && same(l.field, 0, 0, 224, 448) && l.panel_count == 0u);
        int32_t sx = 0, sy = 0;
        OK(sat_tate_edge(&l, 100, 200, &sx, &sy) == SAT_OK && sx == 100 && sy == 200);
        const sat_tate_layout_t w = make(320u, 224u, SAT_TATE_UPRIGHT);
        OK(same(w.field, 104, 0, 112, 224) && w.scale_num == 224u && w.scale_den == 448u);
    }

    /* The corners of a CW and a CCW field. */
    {
        const sat_tate_layout_t cw = make(640u, 224u, SAT_TATE_ROT_CW);
        const sat_tate_layout_t ccw = make(640u, 224u, SAT_TATE_ROT_CCW);
        int32_t x = 0, y = 0;
        /* CW: the logical top-left pixel ends at the screen's top-right of the field. */
        OK(sat_tate_pixel(&cw, 0, 0, &x, &y) == SAT_OK && x == 96 + 447 && y == 0);
        OK(sat_tate_pixel(&cw, 223, 0, &x, &y) == SAT_OK && x == 96 + 447 && y == 223);
        OK(sat_tate_pixel(&cw, 0, 447, &x, &y) == SAT_OK && x == 96 && y == 0);
        OK(sat_tate_pixel(&cw, 223, 447, &x, &y) == SAT_OK && x == 96 && y == 223);
        /* CCW: the logical top-left pixel ends at the screen's bottom-left. */
        OK(sat_tate_pixel(&ccw, 0, 0, &x, &y) == SAT_OK && x == 96 && y == 223);
        OK(sat_tate_pixel(&ccw, 223, 0, &x, &y) == SAT_OK && x == 96 && y == 0);
        OK(sat_tate_pixel(&ccw, 0, 447, &x, &y) == SAT_OK && x == 96 + 447 && y == 223);
        OK(sat_tate_pixel(&ccw, 223, 447, &x, &y) == SAT_OK && x == 96 + 447 && y == 0);
        /* The logical top is the screen's right for CW, left for CCW. */
        sat_rect_t top = {0, 0, 224u, 8u}, r{};
        OK(sat_tate_rect(&cw, &top, &r) == SAT_OK && same(r, 96 + 440, 0, 8, 224));
        OK(sat_tate_rect(&ccw, &top, &r) == SAT_OK && same(r, 96, 0, 8, 224));
    }

    /* Exact layouts: a bijection of pixels onto the field, and rectangles map to the image of their pixels. */
    for (uint8_t rot : {SAT_TATE_UPRIGHT, SAT_TATE_ROT_CW, SAT_TATE_ROT_CCW}) {
        const uint16_t sw = rot == SAT_TATE_UPRIGHT ? 224u : 640u, sh = rot == SAT_TATE_UPRIGHT ? 448u : 224u;
        const sat_tate_layout_t l = make(sw, sh, rot);
        OK(l.exact == 1u);
        std::set<std::pair<int, int>> seen;
        for (int y = 0; y < 448; ++y) {
            for (int x = 0; x < 224; ++x) {
                int32_t sx = 0, sy = 0;
                OK(sat_tate_pixel(&l, x, y, &sx, &sy) == SAT_OK);
                OK(sx >= l.field.x && sx < l.field.x + l.field.width && sy >= l.field.y && sy < l.field.y + l.field.height);
                OK(seen.insert({sx, sy}).second);                                /* never two onto one */
                /* The pixel's own edge maps back to the pixel, or to its neighbour's far edge once rotated. */
                int32_t lx = 0, ly = 0;
                OK(sat_tate_unmap_edge(&l, sx, sy, &lx, &ly) == SAT_OK);
                OK((lx == x || lx == x + 1) && (ly == y || ly == y + 1));
            }
        }
        OK(seen.size() == 224u * 448u && seen.size() == size_t(l.field.width) * l.field.height);

        for (int n = 0; n < 300; ++n) {
            const int w = 1 + rnd() % 60, h = 1 + rnd() % 90;
            const int x = rnd() % (224 - w + 1), y = rnd() % (448 - h + 1);
            const sat_rect_t in = {int16_t(x), int16_t(y), uint16_t(w), uint16_t(h)};
            sat_rect_t out{};
            OK(sat_tate_rect(&l, &in, &out) == SAT_OK);
            OK(rot == SAT_TATE_UPRIGHT ? (out.width == w && out.height == h) : (out.width == h && out.height == w));
            int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
            for (int py = y; py < y + h; ++py)
                for (int px = x; px < x + w; ++px) {
                    int32_t sx = 0, sy = 0;
                    OK(sat_tate_pixel(&l, px, py, &sx, &sy) == SAT_OK);
                    if (sx < x0) x0 = sx;
                    if (sy < y0) y0 = sy;
                    if (sx > x1) x1 = sx;
                    if (sy > y1) y1 = sy;
                }
            OK(out.x == x0 && out.y == y0 && out.x + out.width - 1 == x1 && out.y + out.height - 1 == y1);
        }

        /* Edges round-trip. */
        for (int n = 0; n < 500; ++n) {
            const int lx = rnd() % 225, ly = rnd() % 449;
            int32_t sx = 0, sy = 0, bx = 0, by = 0;
            OK(sat_tate_edge(&l, lx, ly, &sx, &sy) == SAT_OK);
            OK(sat_tate_unmap_edge(&l, sx, sy, &bx, &by) == SAT_OK && bx == lx && by == ly);
        }
    }

    /* Scaled layouts tile: a grid of rectangles covering the field maps onto the screen field with no gap or overlap. */
    for (uint16_t sw : {320u, 352u, 640u}) {
        for (uint8_t rot : {SAT_TATE_ROT_CW, SAT_TATE_ROT_CCW, SAT_TATE_UPRIGHT}) {
            const sat_tate_layout_t l = make(sw, 224u, rot);
            std::vector<int> xs = {0}, ys = {0};
            while (xs.back() < 224) xs.push_back(std::min(224, xs.back() + 1 + int(rnd() % 40)));
            while (ys.back() < 448) ys.push_back(std::min(448, ys.back() + 1 + int(rnd() % 70)));
            std::vector<int> cover(size_t(l.field.width) * l.field.height, 0);
            for (size_t j = 0; j + 1 < ys.size(); ++j)
                for (size_t i = 0; i + 1 < xs.size(); ++i) {
                    const sat_rect_t in = {int16_t(xs[i]), int16_t(ys[j]), uint16_t(xs[i + 1] - xs[i]), uint16_t(ys[j + 1] - ys[j])};
                    sat_rect_t out{};
                    OK(sat_tate_rect(&l, &in, &out) == SAT_OK);
                    for (int y = out.y; y < out.y + out.height; ++y)
                        for (int x = out.x; x < out.x + out.width; ++x) {
                            OK(x >= l.field.x && x < l.field.x + l.field.width && y >= l.field.y && y < l.field.y + l.field.height);
                            ++cover[size_t(y - l.field.y) * l.field.width + (x - l.field.x)];
                        }
                }
            for (int c : cover) OK(c == 1);
            OK(l.field.width <= sw && l.field.height <= 224u);
            /* The whole field maps onto exactly the field rectangle. */
            const sat_rect_t all = {0, 0, 224u, 448u};
            sat_rect_t out{};
            OK(sat_tate_rect(&l, &all, &out) == SAT_OK && same(out, l.field.x, l.field.y, l.field.width, l.field.height));
        }
    }

    /* A sprite rotated offline keeps its place: anchor on the screen minus the packer's rotated pivot
     * lands on the screen rectangle of the logical sprite rectangle. */
    for (uint8_t rot : {SAT_TATE_ROT_CW, SAT_TATE_ROT_CCW}) {
        const sat_tate_layout_t l = make(640u, 224u, rot);
        for (int n = 0; n < 500; ++n) {
            const int w = 8 * (1 + rnd() % 6), h = 1 + rnd() % 40;
            const int px = rnd() % (w + 1), py = rnd() % (h + 1);          /* anchor, in edge coordinates */
            const int lx = rnd() % 225, ly = rnd() % 449;
            const sat_rect_t logical = {int16_t(lx - px), int16_t(ly - py), uint16_t(w), uint16_t(h)};
            sat_rect_t want{};
            OK(sat_tate_rect(&l, &logical, &want) == SAT_OK);
            /* tools/shmup_assets.py rotate_point */
            const int rpx = rot == SAT_TATE_ROT_CW ? h - py : py;
            const int rpy = rot == SAT_TATE_ROT_CW ? px : w - px;
            int32_t ax = 0, ay = 0;
            OK(sat_tate_edge(&l, lx, ly, &ax, &ay) == SAT_OK);
            OK(ax - rpx == want.x && ay - rpy == want.y);
            uint16_t rw = 0, rh = 0;
            sat_tate_size(&l, uint16_t(w), uint16_t(h), &rw, &rh);
            OK(rw == want.width && rh == want.height && rw == h && rh == w);
        }
    }

    /* Invalid input. */
    {
        sat_tate_layout_t l{};
        OK(sat_tate_layout_init(nullptr, 640u, 224u, 224u, 448u, 1u) == SAT_ERR_INVALID_ARG);
        OK(sat_tate_layout_init(&l, 0u, 224u, 224u, 448u, 1u) == SAT_ERR_INVALID_ARG);
        OK(sat_tate_layout_init(&l, 640u, 224u, 0u, 448u, 1u) == SAT_ERR_INVALID_ARG);
        OK(sat_tate_layout_init(&l, 640u, 224u, 224u, 448u, 3u) == SAT_ERR_INVALID_ARG);
        OK(sat_tate_layout_init(&l, 640u, 224u, 224u, 4000u, 1u) == SAT_ERR_INVALID_ARG);
        int32_t a = 0, b = 0;
        OK(sat_tate_edge(&l, 0, 0, &a, &b) == SAT_ERR_INVALID_ARG);          /* uninitialised layout */
        const sat_tate_layout_t ok = make(640u, 224u, SAT_TATE_ROT_CW);
        OK(sat_tate_edge(&ok, 0, 0, nullptr, &b) == SAT_ERR_INVALID_ARG);
    }

    std::puts("tate: OK");
    return 0;
}
