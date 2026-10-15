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

    /* Hardware TATE: rotation parameter table A. Evaluate it the way the VDP2 does (sprX = Xst + y*DXst
     * + x*DX, sprY = Yst + y*DYst + x*DY) and compare with sat_tate_unmap_edge. */
    {
        auto field = [](const uint16_t* t, uint32_t off, uint32_t int_bits) {
            const uint32_t raw = (uint32_t(t[off / 2u]) << 16 | t[off / 2u + 1u]) >> 6;
            const uint32_t bits = int_bits + 10u;
            int32_t v = int32_t(raw & ((1u << bits) - 1u));
            if (v & (1 << (bits - 1u))) v -= 1 << bits;
            return v;                               /* 1/1024 pixel */
        };
        for (uint8_t rot : {SAT_TATE_UPRIGHT, SAT_TATE_ROT_CW, SAT_TATE_ROT_CCW}) {
            for (uint16_t sw : {320u, 352u}) {
                const sat_tate_layout_t l = make(sw, 224u, rot);
                uint16_t t[SAT_TATE_FB_TABLE_WORDS];
                OK(sat_tate_fb_rotation_table(&l, t) == SAT_OK);
                const int32_t xst = field(t, 0x00u, 13u), yst = field(t, 0x04u, 13u);
                const int32_t dxst = field(t, 0x0Cu, 3u), dyst = field(t, 0x10u, 3u);
                const int32_t dx = field(t, 0x14u, 3u), dy = field(t, 0x18u, 3u);
                OK(t[0x1Cu / 2u] == 1u && t[0x1Cu / 2u + 1u] == 0u && t[0x4Cu / 2u] == 1u && t[0x50u / 2u] == 1u);
                for (int sy = 0; sy <= 224; sy += 7) {
                    for (int sx = 0; sx <= int(sw); sx += 5) {
                        if (sx < l.field.x || sy < l.field.y || sx > l.field.x + l.field.width ||
                            sy > l.field.y + l.field.height) continue;
                        const int32_t fx = xst + sy * dxst + sx * dx, fy = yst + sy * dyst + sx * dy;
                        int32_t lx = 0, ly = 0;
                        OK(sat_tate_unmap_edge(&l, sx, sy, &lx, &ly) == SAT_OK);
                        /* unmap floors in the rotated space (so a flipped axis rounds the other way): within a pixel,
                         * plus the rounding of the steps */
                        OK(fx > (lx - 1) * 1024 - 160 && fx < (lx + 1) * 1024 + 160);
                        OK(fy > (ly - 1) * 1024 - 160 && fy < (ly + 1) * 1024 + 160);
                    }
                }
                if (rot == SAT_TATE_ROT_CW && sw == 320u) {     /* the values the working spike used */
                    OK(xst == -45875 && yst == 458752 && dxst == 1434 && dyst == 0 && dx == 0 && dy == -1434);
                }
                if (rot == SAT_TATE_UPRIGHT && sw == 320u) OK(dxst == 0 && dyst == 2048 && dx == 2048);
            }
        }
        /* a field larger than the 512 x 512 buffer is refused, and bad arguments */
        uint16_t t[SAT_TATE_FB_TABLE_WORDS];
        const sat_tate_layout_t big = make(320u, 224u, SAT_TATE_ROT_CW, 224u, 600u);
        OK(sat_tate_fb_rotation_table(&big, t) == SAT_ERR_INVALID_ARG);
        const sat_tate_layout_t ok = make(320u, 224u, SAT_TATE_ROT_CW);
        OK(sat_tate_fb_rotation_table(&ok, nullptr) == SAT_ERR_INVALID_ARG);
        OK(sat_tate_fb_rotation_table(nullptr, t) == SAT_ERR_INVALID_ARG);
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
