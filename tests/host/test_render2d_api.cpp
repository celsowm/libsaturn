#include <cstdio>
#include <cstdlib>

#include "saturn/render2d.h"
#include "src/graphics/2d/palette/registry.hpp"
#include "src/graphics/2d/rendering/runtime.hpp"
#include "src/core/runtime/state.hpp"
#include "src/graphics/2d/textures/runtime.hpp"
#include "src/hal/vdp1/vdp1.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {
uint16_t g_next_srca = 0x2000u;
uint32_t g_palette_uploads = 0u;
uint32_t g_texture_uploads = 0u;
uint32_t g_user_clip_calls = 0u;
uint32_t g_sprite_calls = 0u;
uint32_t g_scaled_calls = 0u;
uint32_t g_distorted_calls = 0u;
uint32_t g_polygon_calls = 0u;
uint32_t g_polyline_calls = 0u;
uint32_t g_line_calls = 0u;
bool g_alpha_configured = false;
uint8_t g_alpha_slot = 4u;
uint8_t g_alpha_requested = 0u;
saturn::hal::vdp1::SpriteRequest g_last_sprite{};
saturn::hal::vdp1::ScaledSpriteRequest g_last_scaled{};
saturn::hal::vdp1::DistortedSpriteRequest g_last_distorted{};
saturn::hal::vdp1::PolygonRequest g_last_polygon{};
saturn::hal::vdp1::PolygonRequest g_last_polyline{};
saturn::hal::vdp1::LineRequest g_last_line{};
saturn::hal::vdp1::UserClipRequest g_last_clip{};
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_alpha_slot(
    uint8_t alpha, uint8_t* out_slot) {
    if (out_slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (!g_alpha_configured) return SAT_ERR_NOT_INITIALIZED;
    g_alpha_requested = alpha;
    *out_slot = g_alpha_slot;
    return SAT_OK;
}

namespace saturn::hal::vdp1 {

sat_result_t upload_palette(const uint16_t*, uint16_t) {
    ++g_palette_uploads;
    return SAT_OK;
}

sat_result_t upload_texture_indexed8_pitched(
    const uint8_t*, uint16_t width, uint16_t height, uint16_t pitch, uint16_t* out_srca
) {
    if (out_srca == nullptr || width == 0u || height == 0u || pitch < width) {
        return SAT_ERR_INVALID_ARG;
    }
    ++g_texture_uploads;
    *out_srca = g_next_srca++;
    return SAT_OK;
}

sat_result_t update_texture_indexed8_pitched(
    uint16_t, const uint8_t*, uint16_t, uint16_t, uint16_t
) {
    return SAT_OK;
}

sat_result_t push_user_clip(const UserClipRequest& req) {
    ++g_user_clip_calls;
    g_last_clip = req;
    return SAT_OK;
}

sat_result_t push_sprite(const SpriteRequest& req) {
    ++g_sprite_calls;
    g_last_sprite = req;
    return SAT_OK;
}

sat_result_t push_scaled_sprite(const ScaledSpriteRequest& req) {
    ++g_scaled_calls;
    g_last_scaled = req;
    return SAT_OK;
}

sat_result_t push_distorted_sprite(const DistortedSpriteRequest& req) {
    ++g_distorted_calls;
    g_last_distorted = req;
    return SAT_OK;
}

sat_result_t push_polygon(const PolygonRequest& req) {
    ++g_polygon_calls;
    g_last_polygon = req;
    return SAT_OK;
}

sat_result_t push_polyline(const PolygonRequest& req) {
    ++g_polyline_calls;
    g_last_polyline = req;
    return SAT_OK;
}

sat_result_t push_line(const LineRequest& req) {
    ++g_line_calls;
    g_last_line = req;
    return SAT_OK;
}

}

static void reset_runtime() {
    using namespace saturn::core;
    g_state = {};
    g_state.initialized = true;
    g_state.config.width = 320u;
    g_state.config.height = 224u;
    g_state.config.ntsc = 1u;
    palette_registry_reset(g_palette_registry);
    texture_registry_reset(g_texture_registry);
    render2d_runtime_reset(g_render2d_runtime);
    g_next_srca = 0x2000u;
    g_palette_uploads = 0u;
    g_texture_uploads = 0u;
    g_user_clip_calls = 0u;
    g_sprite_calls = 0u;
    g_scaled_calls = 0u;
    g_distorted_calls = 0u;
    g_polygon_calls = 0u;
    g_polyline_calls = 0u;
    g_line_calls = 0u;
    g_last_sprite = {};
    g_last_scaled = {};
    g_last_distorted = {};
    g_last_polygon = {};
    g_last_polyline = {};
    g_last_line = {};
    g_last_clip = {};
    g_alpha_configured = false;
    g_alpha_slot = 4u;
    g_alpha_requested = 0u;
}

int main() {
    using namespace saturn::core;
    reset_runtime();

    uint16_t palette[256]{};
    uint8_t pixels[16u * 8u]{};
    sat_surface_t surface{pixels, 16u, 8u, 16u, SAT_PIXEL_INDEX8, palette, 256u};

    sat_texture_t persistent{};
    OK(sat_texture_create_from_surface(
        &persistent, &surface, SAT_TEXTURE_PERSISTENT_SOURCE) == SAT_OK);
    OK(g_texture_uploads == 1u);

    const sat_rect_t full_dst{10, 20, 16u, 8u};
    OK(sat_draw_texture(persistent, nullptr, &full_dst, nullptr) == SAT_OK);
    OK(g_sprite_calls == 1u);
    OK(g_last_sprite.x == -150 && g_last_sprite.y == -92);
    OK(g_last_sprite.width == 16u && g_last_sprite.height == 8u);

    const sat_rect_t src{8, 0, 8u, 8u};
    const sat_rect_t scaled_dst{30, 40, 16u, 16u};
    OK(sat_draw_texture(persistent, &src, &scaled_dst, nullptr) == SAT_OK);
    OK(g_texture_uploads == 2u);
    OK(g_scaled_calls == 1u);
    OK(g_last_scaled.width == 8u && g_last_scaled.height == 8u);

    OK(sat_draw_texture(persistent, &src, &scaled_dst, nullptr) == SAT_OK);
    OK(g_texture_uploads == 2u);
    OK(g_scaled_calls == 2u);

    sat_texture_t upload_only{};
    OK(sat_texture_create_from_surface(
        &upload_only, &surface, SAT_TEXTURE_UPLOAD_ONLY) == SAT_OK);
    OK(sat_draw_texture(upload_only, &src, &scaled_dst, nullptr) == SAT_ERR_UNSUPPORTED);

    sat_draw_params_t rotated = sat_draw_params_default();
    rotated.flip = SAT_FLIP_X;
    rotated.rotation = static_cast<sat_fx16_t>(90 << 16);
    rotated.center.x = 8;
    rotated.center.y = 8;
    OK(sat_draw_texture(persistent, &src, &scaled_dst, &rotated) == SAT_OK);
    OK(g_distorted_calls == 1u);

    sat_camera2d_t camera = sat_camera2d_default();
    camera.offset_x = static_cast<sat_fx16_t>(160 << 16);
    camera.offset_y = static_cast<sat_fx16_t>(112 << 16);
    camera.target_x = static_cast<sat_fx16_t>(10 << 16);
    camera.target_y = static_cast<sat_fx16_t>(20 << 16);
    OK(sat_render2d_set_camera(&camera) == SAT_OK);

    OK(sat_draw_line(
        sat_point_t{10, 20},
        sat_point_t{17, 23},
        sat_color_rgba(255u, 0u, 0u, 255u)) == SAT_OK);
    OK(g_line_calls == 1u);
    OK(g_last_line.x0 == 0 && g_last_line.y0 == 0);
    OK(g_last_line.x1 == 7 && g_last_line.y1 == 3);
    OK(g_last_line.color == SAT_RGB555(31u, 0u, 0u));

    const sat_rect_t rect{10, 20, 8u, 4u};
    OK(sat_draw_rect(&rect, sat_color_rgba(0u, 255u, 0u, 255u)) == SAT_OK);
    OK(g_polyline_calls == 1u);
    OK(g_last_polyline.xa == 0 && g_last_polyline.ya == 0);
    OK(g_last_polyline.color == SAT_RGB555(0u, 31u, 0u));

    const sat_rect_t clip{4, 6, 20u, 30u};
    OK(sat_render2d_set_clip(&clip) == SAT_OK);
    OK(g_user_clip_calls == 0u);
    OK(sat_fill_rect(&rect, sat_color_rgba(0u, 0u, 255u, 255u)) == SAT_OK);
    OK(g_user_clip_calls == 1u);
    OK(g_last_clip.x0 == 4u && g_last_clip.y0 == 6u);
    OK(g_last_clip.x1 == 23u && g_last_clip.y1 == 35u);
    OK(g_polygon_calls == 1u);
    OK(g_last_polygon.user_clip);
    OK(g_last_polygon.color == SAT_RGB555(0u, 0u, 31u));

    OK(sat_fill_rect(&rect, sat_color_rgba(255u, 255u, 255u, 128u)) == SAT_OK);
    OK(g_polygon_calls == 2u);
    OK(g_last_polygon.flags == SAT_SPRITE_FLAG_HALF_TRANSPARENT);
    OK(g_last_polygon.color == SAT_RGB555(31u, 31u, 31u));
    OK(sat_fill_rect(&rect, sat_color_rgba(255u, 255u, 255u, 0u)) == SAT_OK);
    OK(g_polygon_calls == 2u);
    OK(sat_fill_rect(&rect, sat_color_rgba(255u, 255u, 255u, 127u)) == SAT_ERR_UNSUPPORTED);
    /* The earlier camera test deliberately used a non-identity transform.
     * Restore the normal sprite path before checking mesh and alpha routing. */
    sat_camera2d_t default_camera = sat_camera2d_default();
    OK(sat_render2d_set_camera(&default_camera) == SAT_OK);
    sat_draw_params_t meshed = sat_draw_params_default();
    meshed.flags = SAT_SPRITE_FLAG_MESH;
    OK(sat_draw_texture(persistent, nullptr, &full_dst, &meshed) == SAT_OK);
    OK(g_sprite_calls == 2u && g_last_sprite.flags == SAT_SPRITE_FLAG_MESH);

    sat_draw_params_t alpha = sat_draw_params_default();
    alpha.blend_mode = SAT_BLEND_ALPHA;
    alpha.tint.a = 128u;
    OK(sat_draw_texture(persistent, nullptr, &full_dst, &alpha) == SAT_ERR_NOT_INITIALIZED);
    g_alpha_configured = true;
    OK(sat_draw_texture(persistent, nullptr, &full_dst, &alpha) == SAT_OK);
    OK(g_alpha_requested == 128u);
    OK(g_last_sprite.palette == (0x0040u | (4u << 3u)));
    const uint32_t sprites_before_skip = g_sprite_calls;
    alpha.tint.a = 0u;
    OK(sat_draw_texture(persistent, nullptr, &full_dst, &alpha) == SAT_OK);
    OK(g_sprite_calls == sprites_before_skip);
    alpha.tint.a = 255u;
    OK(sat_draw_texture(persistent, nullptr, &full_dst, &alpha) == SAT_OK);
    OK(g_last_sprite.palette == 0u);
    alpha.tint.a = 128u;
    OK(sat_draw_texture(persistent, &src, &scaled_dst, &alpha) == SAT_OK);
    OK(g_last_scaled.palette == (0x0040u | (4u << 3u)));
    alpha.rotation = static_cast<sat_fx16_t>(30 << 16);
    OK(sat_draw_texture(persistent, &src, &scaled_dst, &alpha) == SAT_OK);
    OK(g_last_distorted.palette == (0x0040u | (4u << 3u)));
    std::puts("render2d api: OK");
    return 0;
}
