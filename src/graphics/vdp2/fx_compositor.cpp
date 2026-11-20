#include "saturn/fx_compositor.h"

#include "saturn/color.h"
#include "saturn/dma.h"
#include "saturn/time.h"
#include "saturn/vdp2_alloc.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"
#include "saturn/video.h"
#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/vdp2/vdp2.hpp"

#include <stddef.h>

namespace {
namespace vdp1 = saturn::hal::vdp1;
namespace vdp2 = saturn::hal::vdp2;
constexpr unsigned kRows = 112, kWidth = 160, kStride = 512;
constexpr uint32_t kBitmapBytes = 0x20000;
static_assert(offsetof(sat_fx_compositor_t, rgb) % 4 == 0);
static_assert(offsetof(sat_fx_compositor_t, indexed) % 4 == 0);
sat_fx_compositor_t* owner = nullptr;
uint32_t pending_serial;
uint8_t pending_bitmap;
bool armed;

void build_palette(sat_fx_compositor_t& fx, uint16_t* colors) {
    for (unsigned code = 0; code < 0x8000; ++code) {
        fx.lookup[code] = 0;
        const unsigned r = (code >> 3) & 3u;
        const unsigned g = (code >> 8) & 3u;
        const unsigned b = (code >> 13) & 3u;
        fx.lookup[code | 0x8000u] = static_cast<uint8_t>(1u + 16u * r + 4u * g + b);
    }
    for (unsigned i = 0; i < 64; ++i)
        colors[i + 1] = SAT_RGB555(((i >> 4) & 3u) * 8u, ((i >> 2) & 3u) * 8u, (i & 3u) * 8u);
}

sat_result_t transfer_rows(sat_fx_compositor_t& fx, bool upload, unsigned first, unsigned end) {
    const auto* fb = reinterpret_cast<const uint16_t*>(0x25c80000u);
    auto* vram = reinterpret_cast<uint16_t*>(0x25e00000u + fx.bitmap[pending_bitmap]);
    sat_dma_transfer_t list[SAT_DMA_MAX_LIST];
    for (unsigned row = first; row < end; row += SAT_DMA_MAX_LIST) {
        const unsigned rows = end - row;
        const unsigned count = rows < SAT_DMA_MAX_LIST ? rows : SAT_DMA_MAX_LIST;
        for (unsigned i = 0; i < count; ++i) {
            const unsigned y = row + i;
            if (upload)
                list[i] = {fx.indexed + y * kWidth / 2, vram + y * kStride / 2, kWidth};
            else
                list[i] = {fb + y * kStride + 320, fx.rgb + y * kWidth, kWidth * 2};
        }
        SAT_TRY(sat_dma_start_list(0, list, count));
        SAT_TRY(sat_dma_wait(0));
    }
    return SAT_OK;
}

sat_result_t upload_bitmap(sat_fx_compositor_t& fx) {
    /* If a previous transfer returned early, leave the inactive copy alone
     * until the DMA controller releases its source and destination. */
    if (sat_dma_busy(0)) return SAT_ERR_BUSY;
    const unsigned old_first = fx.bitmap_first_row[pending_bitmap];
    const unsigned old_end = fx.bitmap_end_row[pending_bitmap];
    const unsigned first = old_first < fx.first_row ? old_first : fx.first_row;
    const unsigned end = old_end > fx.end_row ? old_end : fx.end_row;
    const uint64_t start = sat_time_ticks();
    SAT_TRY(transfer_rows(fx, false, first, end));
    const uint64_t read = sat_time_ticks();
    fx.readback_ticks = static_cast<uint32_t>(read - start);
    for (unsigned i = first * kWidth / 2u; i < end * kWidth / 2u; ++i) {
        const uint8_t left = fx.lookup[fx.rgb[2u * i]];
        const uint8_t right = fx.lookup[fx.rgb[2u * i + 1u]];
        fx.indexed[i] = static_cast<uint16_t>((left << 8u) | right);
    }
    SAT_TRY(transfer_rows(fx, true, first, end));
    fx.upload_ticks = static_cast<uint32_t>(sat_time_ticks() - read);
    fx.readback_bytes = (end - first) * kWidth * 2u;
    fx.upload_bytes = (end - first) * kWidth;
    return SAT_OK;
}
}

extern "C" sat_result_t sat_fx_compositor_reserve(sat_fx_compositor_t* fx) {
    if (!fx) return SAT_ERR_INVALID_ARG;
    if (owner || !sat_video_width()) return owner ? SAT_ERR_BUSY : SAT_ERR_NOT_INITIALIZED;
    if (sat_video_width() != 320 || sat_video_height() != 224 || sat_video_is_interlaced())
        return SAT_ERR_UNSUPPORTED;
    saturn::hal::vdp2::nbg::Layer occupied{};
    if (vdp2::nbg_layer(SAT_VDP2_NBG0, &occupied)) return SAT_ERR_BUSY;
    SAT_TRY(sat_vdp2_plan_begin());
    sat_vdp2_palette_range_t palette{};
    sat_result_t st = sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_BITMAP, kBitmapBytes,
                                           kBitmapBytes, &fx->bitmap[0]);
    if (st == SAT_OK)
        st = sat_vdp2_plan_reserve(SAT_VDP2_RESERVE_BITMAP, kBitmapBytes,
                                  kBitmapBytes, &fx->bitmap[1]);
    if (st == SAT_OK) st = sat_vdp2_plan_palette_reserve(256, &palette);
    if (st != SAT_OK) {
        (void)sat_vdp2_plan_rollback();
        return st;
    }
    SAT_TRY(sat_vdp2_plan_commit());
    fx->palette_bank = palette.bank;
    alignas(4) uint16_t colors[256]{};
    build_palette(*fx, colors);
    st = sat_vdp2_palette_upload(colors, 256, palette.bank * 256u);
    sat_vdp2_layer_config_t layer;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &layer);
    layer.bitmap = 1;
    layer.bitmap_size = SAT_VDP2_BITMAP_512X256;
    layer.color_mode = SAT_VDP2_COLOR_MODE_256;
    layer.priority = 7;
    layer.palette = static_cast<uint8_t>(palette.bank);
    layer.plane_address[0] = fx->bitmap[0];
    bool configured = false;
    if (st == SAT_OK) {
        st = sat_vdp2_layer_configure(&layer);
        configured = st == SAT_OK;
    }
    if (st == SAT_OK) st = sat_vdp2_layer_set_zoom(SAT_VDP2_NBG0, 0x20000, 0x20000);
    if (st == SAT_OK) st = sat_vdp2_layer_set_enabled(SAT_VDP2_NBG0, 0);
    if (st != SAT_OK) {
        if (configured) (void)sat_vdp2_layer_release(SAT_VDP2_NBG0);
        (void)sat_vdp2_plan_palette_release(palette);
        (void)sat_vdp2_plan_release(fx->bitmap[1]);
        (void)sat_vdp2_plan_release(fx->bitmap[0]);
        return st;
    }
    fx->visible = fx->prepared = fx->active_bitmap = 0;
    fx->bitmap_first_row[0] = fx->bitmap_first_row[1] = 0;
    fx->bitmap_end_row[0] = fx->bitmap_end_row[1] = kRows;
    owner = fx;
    return SAT_OK;
}

extern "C" sat_result_t sat_fx_compositor_prepare(sat_fx_compositor_t* fx,
    const sat_vdp1_command_checkpoint_t* first, const sat_vdp1_command_checkpoint_t* last,
    const sat_fx_mask_t* masks, uint16_t mask_count) {
    if (!fx || fx != owner || !first || !last) return SAT_ERR_INVALID_ARG;
    if (pending_serial || fx->prepared) return SAT_ERR_BUSY;
    SAT_TRY(vdp1::append_effect_pass(*first, *last, masks, mask_count, fx->first_row, fx->end_row));
    fx->prepared = 1;
    return SAT_OK;
}

extern "C" sat_result_t sat_fx_compositor_present(sat_fx_compositor_t* fx) {
    if (!fx || fx != owner) return SAT_ERR_INVALID_ARG;
    if (!pending_serial) {
        if (!fx->prepared && !fx->visible) return sat_end_frame();
        const uint32_t prior = vdp1::list_serial();
        SAT_TRY(vdp1::hold_next_swap());
        SAT_TRY(sat_end_frame());
        pending_serial = vdp1::list_serial();
        if (pending_serial == prior) {
            pending_serial = 0;
            fx->prepared = 0;
            return SAT_OK; // The SDK dropped the list. Retain the visible pair.
        }
        pending_bitmap = fx->active_bitmap ^ 1u;
    }
    if (!armed) {
        const uint64_t start = sat_time_ticks();
        SAT_TRY(vdp1::wait_held_draw(pending_serial));
        fx->draw_ticks = static_cast<uint32_t>(sat_time_ticks() - start);
        if (fx->prepared) {
            const sat_result_t upload = upload_bitmap(*fx);
            if (upload != SAT_OK) {
                ++fx->recovery_fields;
                SAT_TRY(sat_wait_vblank());
                return upload;
            }
            SAT_TRY(sat_vdp2_layer_set_pattern_name_base(SAT_VDP2_NBG0, fx->bitmap[pending_bitmap]));
        }
        SAT_TRY(sat_vdp2_layer_set_enabled(SAT_VDP2_NBG0, fx->prepared));
        SAT_TRY(sat_wait_vblank());
        SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, fx->prepared, 15));
        SAT_TRY(sat_vdp2_layers_commit());
        SAT_TRY(vdp1::present_held_swap(pending_serial));
        armed = true;
    }
    SAT_TRY(vdp1::wait_presented_swap(pending_serial));
    fx->visible = fx->prepared;
    if (fx->visible) {
        fx->active_bitmap = pending_bitmap;
        fx->bitmap_first_row[pending_bitmap] = fx->first_row;
        fx->bitmap_end_row[pending_bitmap] = fx->end_row;
        ++fx->frames;
    }
    fx->prepared = 0;
    fx->presented_serial = pending_serial;
    pending_serial = 0;
    armed = false;
    return SAT_OK;
}

extern "C" sat_result_t sat_fx_compositor_release(sat_fx_compositor_t* fx) {
    if (!fx || fx != owner) return SAT_ERR_INVALID_ARG;
    if (pending_serial || fx->visible || fx->prepared) return SAT_ERR_BUSY;
    SAT_TRY(sat_vdp2_layer_release(SAT_VDP2_NBG0));
    SAT_TRY(sat_vdp2_plan_release(fx->bitmap[1]));
    SAT_TRY(sat_vdp2_plan_release(fx->bitmap[0]));
    SAT_TRY(sat_vdp2_plan_palette_release((sat_vdp2_palette_range_t){fx->palette_bank, 0, 256}));
    owner = nullptr;
    return SAT_OK;
}
