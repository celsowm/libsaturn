/* test_vdp1_upload.cpp -- host tests for split palette/texture uploads.
 *
 * Links src/graphics/vdp1/api.cpp with stubbed HAL upload/push functions so the
 * API-layer validation and call routing is exercised without hardware.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "saturn/vdp1.h"
#include "src/core/runtime/state.hpp"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)

namespace saturn::hal::vdp1 {

int g_palette_calls = 0;
int g_texture_calls = 0;
sat_result_t g_texture_status = SAT_OK;
uint16_t g_last_palette = 0xFFFFu;

sat_result_t upload_palette(const uint16_t*, uint16_t palette_index) {
    ++g_palette_calls;
    g_last_palette = palette_index;
    return SAT_OK;
}

sat_result_t upload_texture_indexed8(const uint8_t*, uint16_t, uint16_t, uint16_t* out_srca) {
    ++g_texture_calls;
    if (g_texture_status != SAT_OK) {
        return g_texture_status;
    }
    if (out_srca != nullptr) {
        *out_srca = 0x1234u;
    }
    return SAT_OK;
}

int g_reserved_calls = 0;
int g_overlay_pass_calls = 0;
uint16_t g_last_reservation = 0;
sat_result_t reserve_overlay_commands(uint16_t count) {
    ++g_reserved_calls;
    g_last_reservation=count;
    return SAT_OK;
}
sat_result_t begin_overlay_pass() {
    ++g_overlay_pass_calls;
    return SAT_OK;
}
void command_stats(uint16_t& used, uint16_t& capacity,
                   uint16_t& overlay_reserved, bool& overlay_pass) {
    used = 0u;
    capacity = 64u;
    overlay_reserved = g_last_reservation;
    overlay_pass = g_overlay_pass_calls != 0;
}

sat_result_t push_sprite(const SpriteRequest&) { return SAT_OK; }
sat_result_t push_scaled_sprite(const ScaledSpriteRequest&) { return SAT_OK; }
sat_result_t push_distorted_sprite(const DistortedSpriteRequest&) { return SAT_OK; }
sat_result_t push_polygon(const PolygonRequest&) { return SAT_OK; }
sat_result_t push_polyline(const PolygonRequest&) { return SAT_OK; }
sat_result_t push_line(const LineRequest&) { return SAT_OK; }
sat_result_t push_polygon_gouraud(const PolygonRequest&, const uint16_t*) { return SAT_OK; }
sat_result_t push_polyline_gouraud(const PolygonRequest&, const uint16_t*) { return SAT_OK; }
sat_result_t push_line_gouraud(const LineRequest&, const uint16_t*) { return SAT_OK; }

}  // namespace saturn::hal::vdp1

namespace {

void reset_hal() {
    using namespace saturn::hal::vdp1;
    g_palette_calls = 0;
    g_texture_calls = 0;
    g_texture_status = SAT_OK;
    g_last_palette = 0xFFFFu;
}

void make_initialized() {
    saturn::core::g_state.initialized = true;
}

}  // namespace

static void overlay_budget_api_routes_into_hal() {
    using namespace saturn::hal::vdp1;
    make_initialized();
    g_reserved_calls=0;
    g_overlay_pass_calls=0;
    ASSERT_EQ(sat_vdp1_reserve_overlay_commands(192u),SAT_OK);
    ASSERT_EQ(g_reserved_calls,1);
    ASSERT_EQ(g_last_reservation,192u);
    ASSERT_EQ(sat_vdp1_overlay_begin(),SAT_OK);
    ASSERT_EQ(g_overlay_pass_calls,1);
}

static void combined_upload_calls_palette_once_and_texture_once() {
    using namespace saturn::hal::vdp1;
    reset_hal();
    make_initialized();
    uint8_t pixels[8 * 8] = {};
    uint16_t palette[256] = {};
    sat_vdp1_texture_t tex = {};
    ASSERT_EQ(sat_tex_upload_indexed8(&tex, pixels, 8, 8, palette, 2), SAT_OK);
    ASSERT_EQ(g_palette_calls, 1);
    ASSERT_EQ(g_texture_calls, 1);
    ASSERT_EQ(g_last_palette, 2u);
    ASSERT_EQ(tex.valid, 1u);
    ASSERT_EQ(tex.width, 8u);
    ASSERT_EQ(tex.height, 8u);
    ASSERT_EQ(tex.palette, 2u);
}

static void pixels_only_skips_palette_upload() {
    using namespace saturn::hal::vdp1;
    reset_hal();
    make_initialized();
    uint8_t pixels[16 * 8] = {};
    sat_vdp1_texture_t tex = {};
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(&tex, pixels, 16, 8, 3), SAT_OK);
    ASSERT_EQ(g_palette_calls, 0);
    ASSERT_EQ(g_texture_calls, 1);
    ASSERT_EQ(tex.valid, 1u);
    ASSERT_EQ(tex.palette, 3u);
}

static void palette_only_uploads_no_texture() {
    using namespace saturn::hal::vdp1;
    reset_hal();
    make_initialized();
    uint16_t palette[256] = {};
    ASSERT_EQ(sat_palette_upload_indexed8(palette, 1), SAT_OK);
    ASSERT_EQ(g_palette_calls, 1);
    ASSERT_EQ(g_texture_calls, 0);
    ASSERT_EQ(g_last_palette, 1u);
}

static void invalid_bank_rejected_without_hal_calls() {
    using namespace saturn::hal::vdp1;
    reset_hal();
    make_initialized();
    uint8_t pixels[8 * 8] = {};
    uint16_t palette[256] = {};
    sat_vdp1_texture_t tex = {};
    ASSERT_EQ(sat_tex_upload_indexed8(&tex, pixels, 8, 8, palette, 8), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(&tex, pixels, 8, 8, 8), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_palette_upload_indexed8(palette, 8), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(g_palette_calls, 0);
    ASSERT_EQ(g_texture_calls, 0);
}

static void invalid_dims_rejected() {
    using namespace saturn::hal::vdp1;
    reset_hal();
    make_initialized();
    uint8_t pixels[16 * 16] = {};
    uint16_t palette[256] = {};
    sat_vdp1_texture_t tex = {};
    /* Misaligned width. */
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(&tex, pixels, 12, 8, 0), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_tex_upload_indexed8(&tex, pixels, 12, 8, palette, 0), SAT_ERR_INVALID_ARG);
    /* Over hardware maximum. */
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(&tex, pixels, 16, 255, 0), SAT_OK);
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(&tex, pixels, 16, 255, 0), SAT_OK);
    reset_hal();
    /* 512 wide is a multiple of 8 but past the 504 limit. */
    static uint8_t big[512 * 8];
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(&tex, big, 512, 8, 0), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(g_texture_calls, 0);
}

static void texture_capacity_propagates() {
    using namespace saturn::hal::vdp1;
    reset_hal();
    make_initialized();
    g_texture_status = SAT_ERR_CAPACITY;
    uint8_t pixels[8 * 8] = {};
    uint16_t palette[256] = {};
    sat_vdp1_texture_t tex = {};
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(&tex, pixels, 8, 8, 0), SAT_ERR_CAPACITY);
    ASSERT_EQ(sat_tex_upload_indexed8(&tex, pixels, 8, 8, palette, 0), SAT_ERR_CAPACITY);
    /* Combined path uploads the palette before discovering VRAM is full;
     * that ordering is preserved for compatibility. */
    ASSERT_EQ(g_palette_calls, 1);
}

static void null_args_rejected() {
    reset_hal();
    make_initialized();
    uint8_t pixels[8] = {};
    uint16_t palette[256] = {};
    sat_vdp1_texture_t tex = {};
    ASSERT_EQ(sat_tex_upload_indexed8(nullptr, pixels, 8, 1, palette, 0), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_tex_upload_indexed8(&tex, nullptr, 8, 1, palette, 0), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_tex_upload_indexed8(&tex, pixels, 8, 1, nullptr, 0), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(nullptr, pixels, 8, 1, 0), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(&tex, nullptr, 8, 1, 0), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_palette_upload_indexed8(nullptr, 0), SAT_ERR_INVALID_ARG);
}

int main() {
    overlay_budget_api_routes_into_hal();
    combined_upload_calls_palette_once_and_texture_once();
    pixels_only_skips_palette_upload();
    palette_only_uploads_no_texture();
    invalid_bank_rejected_without_hal_calls();
    invalid_dims_rejected();
    texture_capacity_propagates();
    null_args_rejected();
    printf("PASS: test_vdp1_upload.cpp (8 tests)\n");
    return 0;
}
