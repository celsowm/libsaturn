#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "saturn/surface.h"
#include "src/core/surface_logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static bool eq(sat_color_t a, sat_color_t b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

int main() {
    using namespace saturn::core::surface_logic;

    OK(bytes_per_pixel(SAT_PIXEL_INDEX8) == 1u);
    OK(bytes_per_pixel(SAT_PIXEL_RGB555) == 2u);
    OK(bytes_per_pixel(SAT_PIXEL_RGBA8888) == 4u);
    OK(bytes_per_pixel(static_cast<sat_pixel_format_t>(99)) == 0u);

    uint8_t rgba[3 * 16]{};
    sat_surface_t s{};
    OK(init(&s, rgba, 3, 3, 16, SAT_PIXEL_RGBA8888, nullptr, 0) == SAT_OK);
    OK(init(&s, rgba, 3, 3, 11, SAT_PIXEL_RGBA8888, nullptr, 0) == SAT_ERR_INVALID_ARG);

    const uint16_t pal[4] = {
        SAT_RGB555(0, 0, 0),
        SAT_RGB555(31, 0, 0),
        SAT_RGB555(0, 31, 0),
        SAT_RGB555(0, 0, 31)
    };
    uint8_t idx_pixels[8]{};
    sat_surface_t idx{};
    OK(init(&idx, idx_pixels, 4, 2, 4, SAT_PIXEL_INDEX8, pal, 4) == SAT_OK);
    OK(init(&idx, idx_pixels, 4, 2, 4, SAT_PIXEL_INDEX8, nullptr, 0) == SAT_ERR_INVALID_ARG);

    sat_color_t red = sat_color_rgba(255, 0, 0, 255);
    sat_color_t green = sat_color_rgba(0, 255, 0, 255);
    sat_color_t blue = sat_color_rgba(0, 0, 255, 255);
    OK(set_pixel(&idx, 1, 0, red) == SAT_OK);
    OK(idx_pixels[1] == 1u);
    sat_color_t got{};
    OK(get_pixel(&idx, 1, 0, &got) == SAT_OK);
    OK(eq(got, red));
    OK(set_pixel(&idx, 0, 0, sat_color_rgba(10, 20, 30, 255)) == SAT_ERR_UNSUPPORTED);

    sat_rect_t fill_rect{-1, -1, 3, 3};
    OK(fill(&idx, &fill_rect, green) == SAT_OK);
    OK(idx_pixels[0] == 2u && idx_pixels[1] == 2u && idx_pixels[4] == 2u && idx_pixels[5] == 2u);
    OK(idx_pixels[2] == 0u && idx_pixels[6] == 0u);

    uint8_t rgba_pitch[2 * 20]{};
    sat_surface_t p{};
    OK(init(&p, rgba_pitch, 2, 2, 20, SAT_PIXEL_RGBA8888, nullptr, 0) == SAT_OK);
    OK(set_pixel(&p, 1, 1, blue) == SAT_OK);
    OK(rgba_pitch[20 + 4] == 0u && rgba_pitch[20 + 5] == 0u && rgba_pitch[20 + 6] == 255u && rgba_pitch[20 + 7] == 255u);
    OK(rgba_pitch[8] == 0u && rgba_pitch[19] == 0u);

    sat_rect_t sub_rect{1, 0, 1, 2};
    sat_surface_t sub{};
    OK(subview(&p, &sub_rect, &sub) == SAT_OK);
    OK(sub.width == 1u && sub.height == 2u && sub.pitch == 20u);
    OK(get_pixel(&sub, 0, 1, &got) == SAT_OK && eq(got, blue));
    sat_rect_t bad_sub{1, 1, 2, 2};
    OK(subview(&p, &bad_sub, &sub) == SAT_ERR_INVALID_ARG);

    uint16_t rgb555_words[2]{};
    sat_surface_t rgb555{};
    OK(init(&rgb555, rgb555_words, 2, 1, 4, SAT_PIXEL_RGB555, nullptr, 0) == SAT_OK);
    OK(set_pixel(&rgb555, 0, 0, red) == SAT_OK);
    OK(rgb555_words[0] == SAT_RGB555(31, 0, 0));
    OK(get_pixel(&rgb555, 0, 0, &got) == SAT_OK && eq(got, red));

    uint16_t argb_words[2]{};
    sat_surface_t argb{};
    OK(init(&argb, argb_words, 2, 1, 4, SAT_PIXEL_ARGB1555, nullptr, 0) == SAT_OK);
    OK(set_pixel(&argb, 0, 0, sat_color_rgba(255, 0, 0, 0)) == SAT_OK);
    OK((argb_words[0] & 0x8000u) == 0u);
    OK(get_pixel(&argb, 0, 0, &got) == SAT_OK && got.r == 255u && got.a == 0u);

    uint16_t rgb565_words[1]{};
    sat_surface_t rgb565{};
    OK(init(&rgb565, rgb565_words, 1, 1, 2, SAT_PIXEL_RGB565, nullptr, 0) == SAT_OK);
    OK(set_pixel(&rgb565, 0, 0, green) == SAT_OK);
    OK((rgb565_words[0] & 0x07E0u) == 0x07E0u);
    OK(get_pixel(&rgb565, 0, 0, &got) == SAT_OK && got.g == 255u);

    uint8_t conv_rgba[8]{};
    sat_surface_t conv{};
    OK(init(&conv, conv_rgba, 2, 1, 8, SAT_PIXEL_RGBA8888, nullptr, 0) == SAT_OK);
    OK(convert(&conv, &rgb555) == SAT_OK);
    OK(conv_rgba[0] == 255u && conv_rgba[1] == 0u && conv_rgba[2] == 0u && conv_rgba[3] == 255u);

    uint8_t index_src_bytes[4] = {1, 2, 3, 1};
    sat_surface_t index_src{};
    OK(init(&index_src, index_src_bytes, 4, 1, 4, SAT_PIXEL_INDEX8, pal, 4) == SAT_OK);
    uint8_t index_dst_bytes[4]{};
    const uint16_t reordered[4] = {pal[0], pal[3], pal[1], pal[2]};
    sat_surface_t index_dst{};
    OK(init(&index_dst, index_dst_bytes, 4, 1, 4, SAT_PIXEL_INDEX8, reordered, 4) == SAT_OK);
    OK(convert(&index_dst, &index_src) == SAT_OK);
    OK(index_dst_bytes[0] == 2u && index_dst_bytes[1] == 3u && index_dst_bytes[2] == 1u && index_dst_bytes[3] == 2u);

    uint8_t overlap_bytes[6] = {0, 1, 2, 3, 1, 2};
    sat_surface_t overlap{};
    OK(init(&overlap, overlap_bytes, 6, 1, 6, SAT_PIXEL_INDEX8, pal, 4) == SAT_OK);
    sat_rect_t src_shift{0, 0, 5, 1};
    OK(blit(&overlap, sat_point_t{1, 0}, &overlap, &src_shift) == SAT_OK);
    OK(overlap_bytes[0] == 0u && overlap_bytes[1] == 0u && overlap_bytes[2] == 1u && overlap_bytes[3] == 2u && overlap_bytes[4] == 3u && overlap_bytes[5] == 1u);

    uint8_t vertical[9] = {1,2,3, 2,3,1, 3,1,2};
    sat_surface_t vert{};
    OK(init(&vert, vertical, 3, 3, 3, SAT_PIXEL_INDEX8, pal, 4) == SAT_OK);
    sat_rect_t top_two{0, 0, 3, 2};
    OK(blit(&vert, sat_point_t{0, 1}, &vert, &top_two) == SAT_OK);
    OK(vertical[3] == 1u && vertical[4] == 2u && vertical[5] == 3u);
    OK(vertical[6] == 2u && vertical[7] == 3u && vertical[8] == 1u);

    uint8_t clip_src_bytes[4] = {1,2,3,1};
    uint8_t clip_dst_bytes[4] = {0,0,0,0};
    sat_surface_t clip_src{}, clip_dst{};
    OK(init(&clip_src, clip_src_bytes, 2, 2, 2, SAT_PIXEL_INDEX8, pal, 4) == SAT_OK);
    OK(init(&clip_dst, clip_dst_bytes, 2, 2, 2, SAT_PIXEL_INDEX8, pal, 4) == SAT_OK);
    OK(blit(&clip_dst, sat_point_t{-1, -1}, &clip_src, nullptr) == SAT_OK);
    OK(clip_dst_bytes[0] == 1u);
    OK(clip_dst_bytes[1] == 0u && clip_dst_bytes[2] == 0u && clip_dst_bytes[3] == 0u);

    uint8_t small_rgba[2 * 2 * 4] = {
        255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,255,255
    };
    uint8_t big_rgba[4 * 4 * 4]{};
    sat_surface_t small{}, big{};
    OK(init(&small, small_rgba, 2, 2, 8, SAT_PIXEL_RGBA8888, nullptr, 0) == SAT_OK);
    OK(init(&big, big_rgba, 4, 4, 16, SAT_PIXEL_RGBA8888, nullptr, 0) == SAT_OK);
    sat_rect_t all_small{0,0,2,2};
    sat_rect_t all_big{0,0,4,4};
    OK(blit_scaled(&big, &all_big, &small, &all_small) == SAT_OK);
    OK(get_pixel(&big, 0,0,&got) == SAT_OK && eq(got, red));
    OK(get_pixel(&big, 3,0,&got) == SAT_OK && eq(got, green));
    OK(get_pixel(&big, 0,3,&got) == SAT_OK && eq(got, blue));
    OK(get_pixel(&big, 3,3,&got) == SAT_OK && eq(got, sat_color_rgba(255,255,255,255)));

    sat_rect_t scaled_overlap_dst{0,0,2,2};
    OK(blit_scaled(&small, &scaled_overlap_dst, &small, &all_small) == SAT_ERR_UNSUPPORTED);

    std::puts("surface logic ok");
    return 0;
}
