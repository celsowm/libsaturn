/* End-to-end host contract for generated Ikemen KFM assets.
 *
 * The selected standing+crouching subset intentionally contains more than
 * LibSaturn's 64 logical texture slots, so this test validates every sprite
 * with bounded residency instead of uploading the whole character at once.
 */
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "saturn/render2d.h"
#include "src/core/runtime/state.hpp"
#include "src/graphics/2d/palette/registry.hpp"
#include "src/graphics/2d/palette/tint.hpp"
#include "src/graphics/2d/rendering/runtime.hpp"
#include "src/graphics/2d/textures/runtime.hpp"
#include "src/hal/vdp1/vdp1.hpp"

#include "examples/ikemen_saturn/ikemen_anim.h"
#include "ikemen_saturn/kfm_frames.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); std::exit(1); } } while(0)

namespace {
uint32_t g_sprite_calls = 0u;
uint32_t g_distorted_calls = 0u;
}

namespace saturn::hal::vdp1 {
sat_result_t upload_palette(const uint16_t*, uint16_t) { return SAT_OK; }
sat_result_t upload_texture_indexed8_pitched(
    const uint8_t*, uint16_t width, uint16_t height, uint16_t pitch,
    uint16_t* out_srca) {
    if (!width || !height || pitch < width || !out_srca) return SAT_ERR_INVALID_ARG;
    *out_srca = 0x2000u;
    return SAT_OK;
}
bool wait_draw_end() { return true; }
sat_result_t check_texture_indexed8_update(uint16_t,uint16_t,uint16_t,uint16_t){return SAT_OK;}
sat_result_t check_texture_indexed8_rect(uint16_t,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t){return SAT_OK;}
sat_result_t update_texture_indexed8_pitched(uint16_t,const uint8_t*,uint16_t,uint16_t,uint16_t){return SAT_OK;}
sat_result_t update_texture_indexed8_rect(uint16_t,const uint8_t*,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t){return SAT_OK;}
sat_result_t push_sprite(const SpriteRequest&) { ++g_sprite_calls; return SAT_OK; }
sat_result_t push_scaled_sprite(const ScaledSpriteRequest&) { ++g_sprite_calls; return SAT_OK; }
sat_result_t push_distorted_sprite(const DistortedSpriteRequest&) { ++g_distorted_calls; return SAT_OK; }
sat_result_t push_polygon(const PolygonRequest&) { return SAT_OK; }
sat_result_t push_polyline(const PolygonRequest&) { return SAT_OK; }
sat_result_t push_line(const LineRequest&) { return SAT_OK; }
sat_result_t push_user_clip(const UserClipRequest&) { return SAT_OK; }
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_alpha_slot(uint8_t,uint8_t*,uint8_t*) {
    return SAT_ERR_NOT_INITIALIZED;
}
extern "C" sat_result_t sat_vdp2_sprite_color_calc_claim_mode(sat_vdp2_color_calc_mode_t) {
    return SAT_ERR_NOT_INITIALIZED;
}

static std::vector<uint8_t> load_sprite_blob() {
    std::FILE* file = std::fopen(
        "build/generated/ikemen_saturn/iso/KFM_SPR.BIN", "rb");
    OK(file != nullptr);
    OK(std::fseek(file, 0, SEEK_END) == 0);
    const long size = std::ftell(file);
    OK(size >= 0);
    OK(std::fseek(file, 0, SEEK_SET) == 0);
    std::vector<uint8_t> blob(static_cast<size_t>(size));
    OK(blob.empty() ||
       std::fread(blob.data(), 1u, blob.size(), file) == blob.size());
    OK(std::fclose(file) == 0);
    return blob;
}

static sat_texture_t make_texture(const ik_frame_t& frame,
                                  const uint8_t* blob,
                                  uint32_t blob_size) {
    static uint8_t scratch[KFM_MAX_SPRITE_BYTES];
    OK(frame.sprite_index < KFM_SPRITE_COUNT);
    OK(ik_sprite_decode(
        &kfm_sprites[frame.sprite_index],
        blob, blob_size,
        scratch, sizeof(scratch)) != 0);

    sat_surface_t surface{};
    surface.pixels = scratch;
    surface.width = frame.w;
    surface.height = frame.h;
    surface.pitch = frame.w;
    surface.format = SAT_PIXEL_INDEX8;
    surface.palette_rgb555 = kfm_palette_main;
    surface.palette_count = 256u;

    sat_texture_t tex{};
    OK(sat_texture_create_from_surface(
        &tex, &surface, SAT_TEXTURE_UPLOAD_ONLY) == SAT_OK);
    return tex;
}

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

    const ik_frame_table_t table = {
        kfm_frames, KFM_FRAME_COUNT, kfm_clsn_boxes, KFM_CLSN_BOX_COUNT
    };
    const std::vector<uint8_t> sprite_blob = load_sprite_blob();

    OK(sprite_blob.size() == KFM_SPRITE_DATA_BYTES);
    OK(KFM_CLSN_BOX_COUNT > 0u);
    bool saw_attack = false;
    bool saw_hurt = false;
    for (uint32_t i=0;i<table.count;++i) {
        const ik_frame_t& f=kfm_frames[i];
        saw_attack = saw_attack || f.clsn1_count != 0u;
        saw_hurt = saw_hurt || f.clsn2_count != 0u;
        OK((f.w & 7u) == 0u);
        OK(f.w >= 8u && f.w <= 504u);
        OK(f.h >= 1u && f.h <= 255u);
        OK(f.sprite_index < KFM_SPRITE_COUNT);
        const ik_sprite_source_t& sprite = kfm_sprites[f.sprite_index];
        OK(sprite.padded_w == f.w);
        OK(sprite.source_h == f.h);
        OK((sprite.data_ofs & 3u) == 0u);
        OK(sprite.data_ofs <= KFM_SPRITE_DATA_BYTES);
        OK(sprite.data_size <= KFM_SPRITE_DATA_BYTES - sprite.data_ofs);
    }
    OK(KFM_SPRITE_DATA_BYTES < KFM_RAW_PIXELS_BYTES);
    OK(saw_attack && saw_hurt);

    uint32_t unique = 0u;
    for (uint32_t i=0;i<table.count;++i) {
        bool first = true;
        for (uint32_t j=0;j<i;++j) {
            if (kfm_frames[j].sprite_index == kfm_frames[i].sprite_index) {
                first = false;
                break;
            }
        }
        if (first) ++unique;
    }
    /* Regression: pre-uploading all KFM frames no longer fits the registry.
     * The real example must use its bounded frame texture cache. */
    OK(unique > 64u);

    sat_palette_t p2{};
    OK(sat_palette_register(kfm_palette_alt1,&p2) == SAT_OK);

    static const int actions[] = {
        0,11,20,41,105,120,130,200,210,230,240,400,410,430,440
    };
    for (int action : actions) {
        for (uint32_t t=0u;t<40u;t+=7u) {
            const ik_frame_t* frame=ik_frame_at_time(&table,action,t);
            OK(frame != nullptr);
            sat_texture_t tex=make_texture(
                *frame, sprite_blob.data(),
                static_cast<uint32_t>(sprite_blob.size()));

            int16_t dx=0,dy=0;
            ik_frame_screen_anchor(frame,120,178,-1,&dx,&dy);
            sat_draw_params_t params=sat_draw_params_default();
            params.flip=SAT_FLIP_X;
            const sat_rect_t dst{dx,dy,frame->w,frame->h};

            OK(sat_render2d_set_palette(p2) == SAT_OK);
            OK(sat_draw_texture(tex,nullptr,&dst,&params) == SAT_OK);
            OK(sat_render2d_set_palette(sat_palette_none()) == SAT_OK);
            OK(sat_draw_texture(tex,nullptr,&dst,nullptr) == SAT_OK);
            OK(sat_texture_destroy(tex) == SAT_OK);
        }
    }

    OK(g_distorted_calls > 0u);
    OK(g_sprite_calls > 0u);
    std::printf("[test] ikemen_assets OK (%lu unique sprites, bounded residency)\n",
                (unsigned long)unique);
    return 0;
}
