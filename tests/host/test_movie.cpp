#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "saturn/movie.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {

using Bytes = std::vector<uint8_t>;

void put16(Bytes& b, uint32_t v) { b.push_back(static_cast<uint8_t>(v >> 8)); b.push_back(static_cast<uint8_t>(v)); }
void put32(Bytes& b, uint32_t v) { put16(b, v >> 16); put16(b, v & 0xFFFFu); }

Bytes chunk(uint16_t id, const Bytes& body) {
    Bytes c;
    put16(c, static_cast<uint32_t>(id) << 8);   /* the chunk type is the high byte of the id */
    put16(c, static_cast<uint32_t>(body.size() + 4u));
    c.insert(c.end(), body.begin(), body.end());
    return c;
}

/* One frame, one strip. The pixels of an entry are `base + 0..3`. */
Bytes frame(uint32_t width, uint32_t height, const Bytes& chunks, uint32_t strips = 1u) {
    Bytes strip;
    put16(strip, 0x1000u);
    put16(strip, static_cast<uint32_t>(chunks.size() + 12u));
    put16(strip, 0u); put16(strip, 0u); put16(strip, height); put16(strip, width);
    strip.insert(strip.end(), chunks.begin(), chunks.end());
    Bytes f;
    const uint32_t total = static_cast<uint32_t>(strip.size() + 10u);
    f.push_back(0u);
    f.push_back(static_cast<uint8_t>(total >> 16)); f.push_back(static_cast<uint8_t>(total >> 8)); f.push_back(static_cast<uint8_t>(total));
    put16(f, width); put16(f, height); put16(f, strips);
    f.insert(f.end(), strip.begin(), strip.end());
    return f;
}

/* An entry of grey (u = v = 0) with luma 8c, so its dots are 0x8000 | c << 10 | c << 5 | c: c = base .. base + 3. */
Bytes entry(uint32_t base) {
    Bytes e;
    for (uint32_t i = 0u; i < 4u; ++i) e.push_back(static_cast<uint8_t>((base + i) * 8u));
    e.push_back(0u);
    e.push_back(0u);
    return e;
}

uint16_t grey(uint32_t c) { return static_cast<uint16_t>(0x8000u | (c << 10) | (c << 5) | c); }

Bytes book(uint32_t entries, uint32_t first) {
    Bytes b;
    for (uint32_t i = 0u; i < entries; ++i) {
        const Bytes e = entry(first + i * 4u);
        b.insert(b.end(), e.begin(), e.end());
    }
    return b;
}

struct Canvas {
    static constexpr uint32_t kStride = 16u;
    alignas(4) uint16_t px[kStride * 10u];
    Canvas() { std::memset(px, 0, sizeof px); }
    uint16_t at(uint32_t x, uint32_t y) const { return px[y * kStride + x]; }
};

void test_v1_doubles_the_patch() {
    sat_movie_decoder_t d;
    OK(sat_movie_decoder_init(&d, 8u, 8u) == SAT_OK);
    Bytes c = chunk(0x22u, book(2u, 4u));                 /* V1 full: entry 0 = colours 4..7, entry 1 = 8..11 */
    const Bytes vec = chunk(0x32u, Bytes{1u, 0u, 0u, 1u});   /* four V1 blocks: 1, 0, 0, 1 */
    c.insert(c.end(), vec.begin(), vec.end());
    const Bytes f = frame(8u, 8u, c);
    Canvas canvas;
    OK(sat_movie_decode_frame(&d, f.data(), static_cast<uint32_t>(f.size()), canvas.px, Canvas::kStride) == SAT_OK);
    /* Block (0,0) uses entry 1: top-left 2x2 is colour 8, top-right 9, rows 2-3 10 and 11. */
    OK(canvas.at(0, 0) == grey(8) && canvas.at(1, 0) == grey(8) && canvas.at(0, 1) == grey(8) && canvas.at(1, 1) == grey(8));
    OK(canvas.at(2, 0) == grey(9) && canvas.at(3, 1) == grey(9));
    OK(canvas.at(0, 2) == grey(10) && canvas.at(1, 3) == grey(10));
    OK(canvas.at(2, 2) == grey(11) && canvas.at(3, 3) == grey(11));
    /* Block (4,0) uses entry 0, block (0,4) entry 0, block (4,4) entry 1. */
    OK(canvas.at(4, 0) == grey(4) && canvas.at(0, 4) == grey(4) && canvas.at(4, 4) == grey(8));
    OK(canvas.at(8, 0) == 0 && canvas.at(0, 8) == 0);   /* outside the picture: untouched */
}

void test_v4_places_four_patches() {
    sat_movie_decoder_t d;
    OK(sat_movie_decoder_init(&d, 4u, 4u) == SAT_OK);
    Bytes c = chunk(0x20u, book(4u, 0u));                 /* V4 full: entry i = colours 4i .. 4i + 3 */
    Bytes v;
    put32(v, 0x80000000u);                               /* one flag bit: the block is V4 */
    v.push_back(0u); v.push_back(1u); v.push_back(2u); v.push_back(3u);
    const Bytes vec = chunk(0x30u, v);
    c.insert(c.end(), vec.begin(), vec.end());
    const Bytes f = frame(4u, 4u, c);
    Canvas canvas;
    OK(sat_movie_decode_frame(&d, f.data(), static_cast<uint32_t>(f.size()), canvas.px, Canvas::kStride) == SAT_OK);
    OK(canvas.at(0, 0) == grey(0) && canvas.at(1, 0) == grey(1) && canvas.at(0, 1) == grey(2) && canvas.at(1, 1) == grey(3));
    OK(canvas.at(2, 0) == grey(4) && canvas.at(3, 1) == grey(7));
    OK(canvas.at(0, 2) == grey(8) && canvas.at(1, 3) == grey(11));
    OK(canvas.at(2, 2) == grey(12) && canvas.at(3, 3) == grey(15));
}

void test_inter_skips_and_codebook_update_persists() {
    sat_movie_decoder_t d;
    OK(sat_movie_decoder_init(&d, 8u, 4u) == SAT_OK);
    Bytes c = chunk(0x22u, book(2u, 4u));
    const Bytes first = chunk(0x32u, Bytes{0u, 1u});
    c.insert(c.end(), first.begin(), first.end());
    const Bytes f1 = frame(8u, 4u, c);
    Canvas canvas;
    OK(sat_movie_decode_frame(&d, f1.data(), static_cast<uint32_t>(f1.size()), canvas.px, Canvas::kStride) == SAT_OK);
    OK(canvas.at(0, 0) == grey(4) && canvas.at(4, 0) == grey(8));

    /* Frame 2: a V1 update that replaces entry 0 only (a flag word, then that one entry), then an inter
     * chunk. Its flag bits, first to last: block 0 skipped (0), block 1 coded (1), block 1 is V1 (0). */
    Bytes update;
    put32(update, 0x80000000u);
    const Bytes e = entry(20u);
    update.insert(update.end(), e.begin(), e.end());
    Bytes c2 = chunk(0x23u, update);
    Bytes inter;
    put32(inter, 0x40000000u);
    inter.push_back(0u);
    const Bytes vec = chunk(0x31u, inter);
    c2.insert(c2.end(), vec.begin(), vec.end());
    const Bytes f2 = frame(8u, 4u, c2);
    OK(sat_movie_decode_frame(&d, f2.data(), static_cast<uint32_t>(f2.size()), canvas.px, Canvas::kStride) == SAT_OK);
    OK(canvas.at(0, 0) == grey(4) && canvas.at(3, 3) == grey(7));      /* skipped: unchanged */
    OK(canvas.at(4, 0) == grey(20) && canvas.at(7, 3) == grey(23));    /* redrawn from the updated entry 0 */
}

void test_colour_follows_ffmpeg() {
    sat_movie_decoder_t d;
    OK(sat_movie_decoder_init(&d, 4u, 4u) == SAT_OK);
    /* y = 100, 0, 255, 128 with u = -3, v = 5: r = y + 10, g = y + 1 - 5 (u / 2 truncates toward zero: -1), b = y - 6. */
    const Bytes e{100u, 0u, 255u, 128u, 0xFDu, 0x05u};
    Bytes c = chunk(0x22u, e);
    const Bytes vec = chunk(0x32u, Bytes{0u});
    c.insert(c.end(), vec.begin(), vec.end());
    const Bytes f = frame(4u, 4u, c);
    Canvas canvas;
    OK(sat_movie_decode_frame(&d, f.data(), static_cast<uint32_t>(f.size()), canvas.px, Canvas::kStride) == SAT_OK);
    auto dot = [](int r, int g, int b) {
        auto five = [](int v) { return static_cast<uint32_t>((v < 0 ? 0 : v > 255 ? 255 : v) >> 3); };
        return static_cast<uint16_t>(0x8000u | (five(b) << 10) | (five(g) << 5) | five(r));
    };
    OK(canvas.at(0, 0) == dot(110, 100 + 1 - 5, 94));     /* y = 100 */
    OK(canvas.at(2, 0) == dot(10, 0 + 1 - 5, -6));        /* y = 0: g and b clip to 0 */
    OK(canvas.at(0, 2) == dot(255 + 10, 255 + 1 - 5, 249));  /* y = 255: r clips to 255 */
    OK(canvas.at(2, 2) == dot(138, 124, 122));
}

void test_errors() {
    sat_movie_decoder_t d;
    OK(sat_movie_decoder_init(&d, 6u, 8u) == SAT_ERR_INVALID_ARG);
    OK(sat_movie_decoder_init(&d, 8u, 0u) == SAT_ERR_INVALID_ARG);
    OK(sat_movie_decoder_init(&d, 8u, 8u) == SAT_OK);
    Canvas canvas;
    const Bytes two_strips = frame(8u, 8u, Bytes(), 2u);
    OK(sat_movie_decode_frame(&d, two_strips.data(), static_cast<uint32_t>(two_strips.size()), canvas.px, Canvas::kStride) == SAT_ERR_UNSUPPORTED);
    const Bytes wrong = frame(12u, 8u, Bytes());
    OK(sat_movie_decode_frame(&d, wrong.data(), static_cast<uint32_t>(wrong.size()), canvas.px, Canvas::kStride) == SAT_ERR_INVALID_ARG);
    const Bytes grey = frame(8u, 8u, chunk(0x24u, Bytes(8u, 0u)));
    OK(sat_movie_decode_frame(&d, grey.data(), static_cast<uint32_t>(grey.size()), canvas.px, Canvas::kStride) == SAT_ERR_UNSUPPORTED);
    /* V1-only vectors that stop short: four blocks need four index bytes, two are there. */
    Bytes c = chunk(0x22u, book(1u, 4u));
    const Bytes vec = chunk(0x32u, Bytes(2u, 0u));
    c.insert(c.end(), vec.begin(), vec.end());
    const Bytes shortf = frame(8u, 8u, c);
    OK(sat_movie_decode_frame(&d, shortf.data(), static_cast<uint32_t>(shortf.size()), canvas.px, Canvas::kStride) == SAT_ERR_IO);
    OK(sat_movie_decode_frame(&d, shortf.data(), 8u, canvas.px, Canvas::kStride) == SAT_ERR_IO);
    OK(sat_movie_decode_frame(&d, shortf.data(), static_cast<uint32_t>(shortf.size()), canvas.px, 7u) == SAT_ERR_INVALID_ARG);
}

}  // namespace

int main() {
    test_v1_doubles_the_patch();
    test_v4_places_four_patches();
    test_inter_skips_and_codebook_update_persists();
    test_colour_follows_ffmpeg();
    test_errors();
    std::puts("test_movie: OK");
    return 0;
}
