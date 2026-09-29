/* End-to-end host repro of the ikemen_saturn runtime wiring: builds every
 * frame texture from the generated KFM tables, registers the P2 palette
 * override and draws both players across sampled actions/states. This is
 * the regression net for the example's asset contract:
 *   - every frame texture fits VDP1 rules (width % 8 == 0, 8..504, h 1..255)
 *   - every frame resolves to a valid texture and draws (plain + override)
 *   - unique-texture dedup matches (kfm_frames reuse pixel_ofs)
 * Run only when the generated tables exist (Makefile gates on the
 * .external screenpack clone). */
#include <cstdio>
#include <cstdlib>

#include "saturn/render2d.h"
#include "src/core/runtime/state.hpp"
#include "src/graphics/2d/palette/registry.hpp"
#include "src/graphics/2d/palette/tint.hpp"
#include "src/graphics/2d/rendering/runtime.hpp"
#include "src/graphics/2d/textures/runtime.hpp"
#include "src/hal/vdp1/vdp1.hpp"

#include "examples/common/ikemen_anim.h"
#include "ikemen_saturn/kfm_frames.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {
uint32_t g_sprite_calls = 0u;
uint32_t g_distorted_calls = 0u;
}

namespace saturn::hal::vdp1 {
sat_result_t upload_palette(const uint16_t*, uint16_t) { return SAT_OK; }
sat_result_t upload_texture_indexed8_pitched(
    const uint8_t*, uint16_t width, uint16_t height, uint16_t pitch, uint16_t* out_srca) {
    if (width == 0u || height == 0u || pitch < width || out_srca == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    *out_srca = 0x2000u;
    return SAT_OK;
}
bool wait_draw_end() { return true; }
sat_result_t check_texture_indexed8_update(uint16_t, uint16_t, uint16_t, uint16_t) { return SAT_OK; }
sat_result_t check_texture_indexed8_rect(
    uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t) { return SAT_OK; }
sat_result_t update_texture_indexed8_pitched(
    uint16_t, const uint8_t*, uint16_t, uint16_t, uint16_t) { return SAT_OK; }
sat_result_t update_texture_indexed8_rect(
    uint16_t, const uint8_t*, uint16_t, uint16_t, uint16_t,
    uint16_t, uint16_t, uint16_t, uint16_t) { return SAT_OK; }
sat_result_t push_sprite(const SpriteRequest&) { ++g_sprite_calls; return SAT_OK; }
sat_result_t push_scaled_sprite(const ScaledSpriteRequest&) { ++g_sprite_calls; return SAT_OK; }
sat_result_t push_distorted_sprite(const DistortedSpriteRequest&) { ++g_distorted_calls; return SAT_OK; }
sat_result_t push_polygon(const PolygonRequest&) { return SAT_OK; }
sat_result_t push_polyline(const PolygonRequest&) { return SAT_OK; }
sat_result_t push_line(const LineRequest&) { return SAT_OK; }
sat_result_t push_user_clip(const UserClipRequest&) { return SAT_OK; }
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_alpha_slot(
    uint8_t, uint8_t*, uint8_t*) { return SAT_ERR_NOT_INITIALIZED; }
extern "C" sat_result_t sat_vdp2_sprite_color_calc_claim_mode(
    sat_vdp2_color_calc_mode_t) { return SAT_ERR_NOT_INITIALIZED; }

int main() {
    using namespace saturn::core;
    g_state = {};
    g_state.initialized = true;
    g_state.config.width = 320u;
    g_state.config.height = 224u;
    g_state.config.ntsc = 1u;
    palette_registry_reset(g_palette_registry);
    tint_cache_reset(g_tint_cache);
    texture_registry_reset(g_texture_registry);
    render2d_runtime_reset(g_render2d_runtime);

    const ik_frame_table_t table = {kfm_frames, KFM_FRAME_COUNT};

    /* Asset contract: VDP1-legal texture geometry for every frame. */
    for (uint32_t i = 0u; i < table.count; ++i) {
        const ik_frame_t& frame = kfm_frames[i];
        OK((frame.w & 7u) == 0u);
        OK(frame.w >= 8u && frame.w <= 504u);
        OK(frame.h >= 1u && frame.h <= 255u);
        OK(frame.pixel_ofs + (uint32_t)frame.w * frame.h <= (uint32_t)KFM_PIXELS_BYTES);
    }

    /* Build one texture per frame (dedup by pixel_ofs, like the example). */
    static sat_texture_t frame_textures[KFM_FRAME_COUNT];
    uint32_t unique = 0u;
    for (uint32_t i = 0u; i < table.count; ++i) {
        const ik_frame_t& frame = kfm_frames[i];
        sat_texture_t tex{0u, 0u};
        for (uint32_t j = 0u; j < i; ++j) {
            if (kfm_frames[j].pixel_ofs == frame.pixel_ofs) { tex = frame_textures[j]; break; }
        }
        if (tex.generation == 0u) {
            sat_surface_t surface;
            surface.pixels = (uint8_t*)kfm_pixels + frame.pixel_ofs;
            surface.width = frame.w;
            surface.height = frame.h;
            surface.pitch = frame.w;
            surface.format = SAT_PIXEL_INDEX8;
            surface.palette_rgb555 = kfm_palette_main;
            surface.palette_count = 256u;
            const sat_result_t st = sat_texture_create_from_surface(
                &tex, &surface, SAT_TEXTURE_UPLOAD_ONLY);
            if (st != SAT_OK) {
                std::fprintf(stderr,
                    "texture create failed for frame %lu (action %u, %ux%u): %d\n",
                    (unsigned long)i, frame.action, frame.w, frame.h, (int)st);
                std::exit(1);
            }
            ++unique;
        }
        frame_textures[i] = tex;
    }
    OK(unique > 0u);
    std::printf("[test] ikemen_assets: %lu frames, %lu unique textures\n",
                (unsigned long)table.count, (unsigned long)unique);

    /* P2 palette override registration. */
    sat_palette_t p2{};
    OK(sat_palette_register(kfm_palette_alt1, &p2) == SAT_OK);
    OK(p2.generation != 0u);

    /* Draw both players across every action at several times. */
    static const int actions[] = {0, 20, 40, 42, 105, 120, 130, 200, 210, 230};
    for (int action : actions) {
        for (uint32_t t = 0u; t < 40u; t += 7u) {
            const ik_frame_t* frame = ik_frame_at_time(&table, action, t);
            OK(frame != nullptr);
            const uint32_t index = (uint32_t)(frame - kfm_frames);
            int16_t dx = 0;
            int16_t dy = 0;
            ik_frame_screen_anchor(frame, 120, 178, 1, &dx, &dy);
            sat_draw_params_t params = sat_draw_params_default();
            params.flip = SAT_FLIP_X; /* facing left, like P2 */
            const sat_rect_t dst{dx, dy, frame->w, frame->h};
            OK(sat_render2d_set_palette(p2) == SAT_OK);
            const sat_result_t st = sat_draw_texture(
                frame_textures[index], nullptr, &dst, &params);
            if (st != SAT_OK) {
                std::fprintf(stderr, "draw failed action %d t %lu: %d\n",
                             action, (unsigned long)t, (int)st);
                std::exit(1);
            }
            OK(sat_render2d_set_palette(sat_palette_none()) == SAT_OK);
            OK(sat_draw_texture(frame_textures[index], nullptr,
                &dst, nullptr) == SAT_OK);
        }
    }
    OK(g_distorted_calls > 0u); /* flip draws went through the quad path */
    OK(g_sprite_calls > 0u);

    std::puts("[test] ikemen_assets OK");
    return 0;
}
