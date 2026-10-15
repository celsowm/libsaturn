#include "saturn/tate_fb.h"

#include "saturn/vdp2.h"
#include "src/core/runtime/state.hpp"
#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace {

bool g_active = false;

constexpr uint32_t kBitmapWords = 512u * 256u / 2u;    /* 512 x 256 x 8 bits */
constexpr uint32_t kBank128KWords = 0x10000u;

}  // namespace

extern "C" sat_result_t sat_tate_fb_init(const sat_tate_layout_t* layout, uint32_t bitmap_word, uint32_t table_word) {
    using namespace saturn::core;
    const sat_result_t init = require_initialized();
    if (init != SAT_OK) return init;
    if (layout == nullptr || g_active) return SAT_ERR_INVALID_ARG;
    if (g_state.config.width != 320u || g_state.config.height != 224u) return SAT_ERR_UNSUPPORTED;
    if (layout->screen_w != g_state.config.width || layout->screen_h != g_state.config.height) return SAT_ERR_INVALID_ARG;
    /* The bitmap and the table must not share a 128 KiB bank, or the table would be read as pixels. */
    if ((bitmap_word & (kBank128KWords - 1u)) != 0u || (bitmap_word / kBank128KWords) == (table_word / kBank128KWords)) {
        return SAT_ERR_INVALID_ARG;
    }

    uint16_t table[SAT_TATE_FB_TABLE_WORDS];
    SAT_TRY(sat_tate_fb_rotation_table(layout, table));

    /* The dummy RBG0 plane: VRAM is not zero at power on, and a non-zero pixel would draw. */
    static const uint16_t zeros[256] = {0};
    for (uint32_t w = 0; w < kBitmapWords; w += 256u) {
        SAT_TRY(sat_vdp2_vram_write_words(bitmap_word + w, zeros, 256u));
    }
    SAT_TRY(sat_vdp2_vram_write_words(table_word, table, SAT_TATE_FB_TABLE_WORDS));

    const sat_vdp2_rbg0_config_t rbg0 = {SAT_VDP2_RBG0_BITMAP_512x256, SAT_VDP2_COLOR_MODE_256, bitmap_word, table_word};
    SAT_TRY(sat_vdp2_rbg0_init(&rbg0));
    SAT_TRY(sat_vdp2_rbg0_set_transparent_code_enabled(1u));
    SAT_TRY(sat_vdp2_rbg0_set_screen_over(SAT_VDP2_RBG0_OVER_TRANSPARENT));   /* the bitmap is 256 lines, the frame buffer 512 */
    SAT_TRY(sat_vdp2_rbg0_set_priority(1u));
    SAT_TRY(sat_vdp2_rbg0_set_sprite_priority(7u));

    saturn::hal::vdp1::set_fb_rotation(layout->logical_w, layout->logical_h);
    saturn::hal::vdp2::set_sprite_fb8(true);
    g_state.config.width = layout->logical_w;
    g_state.config.height = layout->logical_h;
    g_active = true;
    return sat_vdp2_layers_commit();
}

extern "C" uint8_t sat_tate_fb_active(void) {
    return g_active ? 1u : 0u;
}
