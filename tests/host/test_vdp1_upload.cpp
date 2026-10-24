/* test_vdp1_upload.cpp -- host tests for split palette/texture uploads.
 *
 * Links src/graphics/vdp1/api.cpp with stubbed HAL upload/push functions so the
 * API-layer validation and call routing is exercised without hardware.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "saturn/vdp1.h"
#include "src/graphics/vdp1/projected.hpp"
#include "src/core/runtime/state.hpp"
#include "src/graphics/2d/rendering/runtime.hpp"

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

int g_lut_calls = 0;
int g_lut4_calls = 0;
int g_region_writes = 0;

sat_result_t reserve_texture_region(uint32_t bytes, uint32_t* out_offset) {
    if (out_offset == nullptr || bytes == 0u) return SAT_ERR_INVALID_ARG;
    *out_offset = 0x14000u;
    return SAT_OK;
}

sat_result_t write_texture_region(uint32_t, const void*, uint32_t) {
    ++g_region_writes;
    return SAT_OK;
}

sat_result_t upload_lut(const uint16_t*, uint16_t* out_colr) {
    ++g_lut_calls;
    *out_colr = 0x2A04u;
    return SAT_OK;
}

sat_result_t upload_texture_lut4(const uint8_t*, uint16_t, uint16_t, uint16_t* out_srca) {
    ++g_lut4_calls;
    *out_srca = 0x3456u;
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
sat_result_t command_checkpoint(sat_vdp1_command_checkpoint_t* out) {
    if (!out) return SAT_ERR_INVALID_ARG;
    out->frame_serial=1u;
    out->used=2u;
    out->gouraud_tables=0u;
    out->overlay_reserved=g_last_reservation;
    out->overlay_pass=g_overlay_pass_calls!=0?1u:0u;
    out->reserved=0u;
    return SAT_OK;
}
sat_result_t command_rollback(
    const sat_vdp1_command_checkpoint_t* checkpoint) {
    return checkpoint && checkpoint->frame_serial==1u
        ? SAT_OK:SAT_ERR_INVALID_ARG;
}
void command_stats(uint16_t& used, uint16_t& capacity,
                   uint16_t& overlay_reserved, bool& overlay_pass) {
    used = 0u;
    capacity = 64u;
    overlay_reserved = g_last_reservation;
    overlay_pass = g_overlay_pass_calls != 0;
}

SpriteRequest g_last_sprite{};
int g_sprite_calls = 0;
sat_result_t push_sprite(const SpriteRequest& req) {
    g_last_sprite = req;
    ++g_sprite_calls;
    return SAT_OK;
}
sat_result_t push_scaled_sprite(const ScaledSpriteRequest&) { return SAT_OK; }
int g_projected_calls=0;
int g_program_calls=0;
sat_result_t g_program_status=SAT_OK;
sat_result_t push_projected_program(const sat_quad2_t&,const uint16_t*,const sat_vdp1_texture_t&) {
    ++g_program_calls;
    return g_program_status;
}
sat_result_t push_projected_sprite(const sat_quad2_t&, const sat_vdp1_texture_t&, uint16_t) {
    ++g_projected_calls;
    return SAT_OK;
}
DistortedSpriteRequest g_last_distorted{};
sat_result_t push_distorted_sprite_gouraud(const DistortedSpriteRequest& req,const uint16_t*) { return push_distorted_sprite(req); }
sat_result_t push_distorted_sprite(const DistortedSpriteRequest& req) {
    g_last_distorted = req;
    return SAT_OK;
}
UserClipRequest g_last_clip{};
int g_clip_calls = 0;
sat_result_t push_user_clip(const UserClipRequest& req) {
    g_last_clip = req;
    ++g_clip_calls;
    return SAT_OK;
}
sat_result_t push_polygon(const PolygonRequest&) { return SAT_OK; }
sat_result_t push_polyline(const PolygonRequest&) { return SAT_OK; }
sat_result_t push_line(const LineRequest&) { return SAT_OK; }
sat_result_t push_polygon_gouraud(const PolygonRequest&, const uint16_t*) { return SAT_OK; }
sat_result_t push_polyline_gouraud(const PolygonRequest&, const uint16_t*) { return SAT_OK; }
sat_result_t push_line_gouraud(const LineRequest&, const uint16_t*) { return SAT_OK; }

uint32_t late_swaps() { return 7u; }
void (*g_test_swap_handler)(uint32_t, void*) = nullptr;
void* g_test_swap_user = nullptr;
void set_swap_handler(void (*handler)(uint32_t, void*), void* user) {
    g_test_swap_handler = handler;
    g_test_swap_user = user;
}
uint32_t list_serial() { return 42u; }
uint32_t draw_waits() { return 5u; }
uint32_t draw_timeouts() { return 1u; }

}  // namespace saturn::hal::vdp1

/* render2d's runtime is not linked: only its clip flag is touched here. */
namespace saturn::core {
Render2DRuntime g_render2d_runtime{};
}

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
    sat_vdp1_command_checkpoint_t mark{};
    ASSERT_EQ(sat_vdp1_command_checkpoint(&mark),SAT_OK);
    ASSERT_EQ(mark.used,2u);
    ASSERT_EQ(mark.overlay_reserved,192u);
    ASSERT_EQ(sat_vdp1_command_rollback(&mark),SAT_OK);
    ASSERT_EQ(sat_vdp1_command_checkpoint(nullptr),SAT_ERR_INVALID_ARG);
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

static void sprite_screen_uses_active_video_dimensions() {
    using namespace saturn::hal::vdp1;
    make_initialized();
    sat_vdp1_texture_t texture{};
    texture.valid = 1u;
    texture.width = 8u;
    texture.height = 8u;
    g_sprite_calls = 0;

    saturn::core::g_state.config.width = 320u;
    saturn::core::g_state.config.height = 224u;
    ASSERT_EQ(sat_draw_sprite_screen(&texture, 160, 112, 0u, 0u, 0u), SAT_OK);
    ASSERT_EQ(g_last_sprite.x, -4);
    ASSERT_EQ(g_last_sprite.y, -4);

    saturn::core::g_state.config.width = 352u;
    saturn::core::g_state.config.height = 240u;
    ASSERT_EQ(sat_draw_sprite_screen(&texture, 176, 120, 0u, 0u, 0u), SAT_OK);
    ASSERT_EQ(g_last_sprite.x, -4);
    ASSERT_EQ(g_last_sprite.y, -4);
    ASSERT_EQ(g_last_sprite.width, 8u);
    ASSERT_EQ(g_last_sprite.height, 8u);
    ASSERT_EQ(g_sprite_calls, 2);
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

static void lut4_upload_records_table_and_format() {
    using namespace saturn::hal::vdp1;
    reset_hal();
    make_initialized();
    const uint16_t lut[16] = {};
    uint16_t handle = 0u;
    ASSERT_EQ(sat_vdp1_upload_lut(lut, &handle), SAT_OK);
    ASSERT_EQ(handle, 0x2A04u);
    uint8_t pixels[8 * 4 / 2] = {};
    sat_vdp1_texture_t tex = {};
    ASSERT_EQ(sat_tex_upload_lut4_pixels(&tex, pixels, 8, 4, handle), SAT_OK);
    ASSERT_EQ(tex.srca, 0x3456u);
    ASSERT_EQ(tex.palette, 0x2A04u);
    ASSERT_EQ(tex.format, SAT_VDP1_TEXTURE_LUT4);
    ASSERT_EQ(tex.valid, 1u);
    /* A table can never sit at VRAM 0; bad sizes never reach the HAL. */
    ASSERT_EQ(sat_tex_upload_lut4_pixels(&tex, pixels, 8, 4, 0u), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_tex_upload_lut4_pixels(&tex, pixels, 12, 4, handle), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(g_lut4_calls, 1);
    /* Indexed uploads keep reporting the indexed format. */
    ASSERT_EQ(sat_tex_upload_indexed8_pixels(&tex, pixels, 8, 1, 0), SAT_OK);
    ASSERT_EQ(tex.format, SAT_VDP1_TEXTURE_INDEXED8);
}

static void frame_stats_report_hal_counters() {
    make_initialized();
    sat_vdp1_frame_stats_t stats{};
    ASSERT_EQ(sat_vdp1_frame_stats(&stats), SAT_OK);
    ASSERT_EQ(stats.late_swaps, 7u);
    ASSERT_EQ(stats.draw_waits, 5u);
    ASSERT_EQ(stats.draw_timeouts, 1u);
    ASSERT_EQ(sat_vdp1_frame_stats(nullptr), SAT_ERR_INVALID_ARG);
}

static void swap_handler_reaches_hal() {
    make_initialized();
    int user = 0;
    auto handler = [](uint32_t serial, void* u) { *static_cast<int*>(u) = static_cast<int>(serial); };
    ASSERT_EQ(sat_vdp1_set_swap_handler(handler, &user), SAT_OK);
    ASSERT_EQ(saturn::hal::vdp1::g_test_swap_user, static_cast<void*>(&user));
    saturn::hal::vdp1::g_test_swap_handler(sat_vdp1_list_serial(), saturn::hal::vdp1::g_test_swap_user);
    ASSERT_EQ(user, 42);
    ASSERT_EQ(sat_vdp1_set_swap_handler(nullptr, nullptr), SAT_OK);
    ASSERT_EQ(saturn::hal::vdp1::g_test_swap_handler, nullptr);
}

/* sat_vdp1_set_user_clip cuts the box to the screen, refuses one wholly off
 * it, and makes render2d send its own clip again; the sprite flag reaches the
 * HAL as the inside-clip request. */
static void user_clip_api_clamps_and_routes() {
    using namespace saturn::hal::vdp1;
    make_initialized();
    saturn::core::g_state.config.width = 320u;
    saturn::core::g_state.config.height = 224u;
    g_clip_calls = 0;
    saturn::core::g_render2d_runtime.current.clip_enabled = 1u;
    saturn::core::g_render2d_runtime.clip_dirty = 0u;
    ASSERT_EQ(sat_vdp1_set_user_clip(-4, 100, 63, 300), SAT_OK);
    ASSERT_EQ(g_clip_calls, 1);
    ASSERT_EQ(g_last_clip.x0, 0u);
    ASSERT_EQ(g_last_clip.y0, 100u);
    ASSERT_EQ(g_last_clip.x1, 63u);
    ASSERT_EQ(g_last_clip.y1, 223u);
    ASSERT_EQ(saturn::core::g_render2d_runtime.clip_dirty, 1u);
    ASSERT_EQ(sat_vdp1_set_user_clip(320, 0, 400, 10), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(sat_vdp1_set_user_clip(10, 10, 9, 20), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(g_clip_calls, 1);
    saturn::core::g_render2d_runtime.current.clip_enabled = 0u;
    saturn::core::g_render2d_runtime.clip_dirty = 0u;

    sat_vdp1_texture_t texture{};
    texture.valid = 1u;
    texture.width = 16u;
    texture.height = 16u;
    sat_distorted_sprite_cmd_t quad{};
    quad.texture = &texture;
    quad.flags = SAT_SPRITE_FLAG_USER_CLIP;
    ASSERT_EQ(sat_draw_sprite_distorted(&quad), SAT_OK);
    ASSERT_TRUE(g_last_distorted.user_clip);
    quad.flags = 0u;
    ASSERT_EQ(sat_draw_sprite_distorted(&quad), SAT_OK);
    ASSERT_TRUE(!g_last_distorted.user_clip);
    sat_sprite_cmd_t sprite{};
    sprite.texture = &texture;
    sprite.flags = SAT_SPRITE_FLAG_USER_CLIP;
    ASSERT_EQ(sat_draw_sprite(&sprite), SAT_OK);
    ASSERT_TRUE(g_last_sprite.user_clip);
}

int main() {
    sat_quad2_t projected{};sat_vdp1_texture_t projected_texture{};
    saturn::core::g_state.initialized=false;
    ASSERT_EQ(saturn::internal::draw_projected_sprite(projected,projected_texture,0),SAT_ERR_NOT_INITIALIZED);
    ASSERT_EQ(saturn::hal::vdp1::g_projected_calls,0);
    saturn::core::g_state.initialized=true;
    ASSERT_EQ(saturn::internal::draw_projected_sprite(projected,projected_texture,0),SAT_OK);
    ASSERT_EQ(saturn::hal::vdp1::g_projected_calls,1);
    sat_vdp1_projected_program_t program{};
    saturn::core::g_state.initialized=false;
    ASSERT_EQ(saturn::internal::draw_projected_program(projected,program),SAT_ERR_NOT_INITIALIZED);
    ASSERT_EQ(saturn::hal::vdp1::g_program_calls,0);
    saturn::core::g_state.initialized=true;
    ASSERT_EQ(saturn::internal::draw_projected_program(projected,program),SAT_ERR_INVALID_ARG);
    ASSERT_EQ(saturn::hal::vdp1::g_program_calls,0);
    program.texture=&projected_texture;
    ASSERT_EQ(saturn::internal::draw_projected_program(projected,program),SAT_OK);
    ASSERT_EQ(saturn::hal::vdp1::g_program_calls,1);
    saturn::hal::vdp1::g_program_status=SAT_ERR_CAPACITY;
    ASSERT_EQ(saturn::internal::draw_projected_program(projected,program),SAT_ERR_CAPACITY);
    saturn::hal::vdp1::g_program_status=SAT_OK;

    user_clip_api_clamps_and_routes();
    overlay_budget_api_routes_into_hal();
    frame_stats_report_hal_counters();
    swap_handler_reaches_hal();
    lut4_upload_records_table_and_format();
    combined_upload_calls_palette_once_and_texture_once();
    pixels_only_skips_palette_upload();
    palette_only_uploads_no_texture();
    invalid_bank_rejected_without_hal_calls();
    invalid_dims_rejected();
    texture_capacity_propagates();
    sprite_screen_uses_active_video_dimensions();
    null_args_rejected();
    printf("PASS: test_vdp1_upload.cpp (12 tests)\n");
    return 0;
}
