#include "saturn/vdp2.h"

#include "src/core/logic.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/vdp2.hpp"

extern "C" sat_result_t sat_vdp2_nbg0_init(const sat_vdp2_nbg0_config_t* config) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    st = validate_nbg0_config(config);
    if (st != SAT_OK) {
        return st;
    }

    /* Save state */
    g_state.nbg0_map_plane_index = config->map_plane_index;
    g_state.nbg0_map_width = 64u;
    g_state.nbg0_map_height = 64u;

    /* Direct register write sequence matching the working vdp2_nbg0_image.
     * This bypasses the broken configure_nbg0_text_layout which was not
     * enabling NBG0 correctly. */
    volatile uint16_t* const regs = (volatile uint16_t*)0x25F80000u;

    regs[0x000u >> 1] = 0x0000u;  /* TVMD off */
    regs[0x00Eu >> 1] = 0x1327u;  /* RAMCTL */

    regs[0x010u >> 1] = 0x5555u;
    regs[0x012u >> 1] = 0xFEEEu;
    regs[0x014u >> 1] = 0x5555u;
    regs[0x016u >> 1] = 0xFEEEu;
    regs[0x018u >> 1] = 0xFFFFu;
    regs[0x01Au >> 1] = 0xEEEEu;
    regs[0x01Cu >> 1] = 0x044Fu;
    regs[0x01Eu >> 1] = 0xEEEEu;

    regs[0x020u >> 1] = 0x0000u;  /* BGON off during config */

    /* CHCTLA: color mode + char size */
    const uint16_t mode = static_cast<uint16_t>(config->color_mode) & 0x0007u;
    const uint16_t chctl = static_cast<uint16_t>(0x3200u | (mode << 4u) |
        (config->char_size == SAT_VDP2_CHAR_SIZE_2X2 ? 0x0001u : 0u));
    regs[0x028u >> 1] = chctl;

    regs[0x030u >> 1] = 0xC00Cu;  /* PNCN0 */
    regs[0x03Au >> 1] = 0x0000u;  /* PLSZ */
    regs[0x03Cu >> 1] = 0x0000u;  /* MPOFN */

    /* MPABN0/MPCDN0: map plane index */
    const uint16_t mp = static_cast<uint16_t>(config->map_plane_index & 0x003Fu);
    const uint16_t mp_packed = static_cast<uint16_t>((mp << 8u) | mp);
    regs[0x040u >> 1] = mp_packed;
    regs[0x042u >> 1] = mp_packed;

    regs[0x078u >> 1] = 0x0001u;
    regs[0x07Au >> 1] = 0x0000u;
    regs[0x07Cu >> 1] = 0x0001u;
    regs[0x07Eu >> 1] = 0x0000u;

    regs[0x0F0u >> 1] = 0x0606u;  /* PRISA */
    regs[0x0F8u >> 1] = 0x0607u;  /* PRINA */

    /* BGON: bit 0 = NBG0 enable, bit 8 = transparent code disable */
    const uint16_t bgon = config->transparent_code_enabled ? 0x0001u : 0x0101u;
    regs[0x020u >> 1] = bgon;
    regs[0x000u >> 1] = 0x8100u;  /* TVMD on */

    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_set_scroll(const sat_vdp2_scroll_t* scroll) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (scroll == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    saturn::hal::vdp2::set_nbg0_scroll(
        scroll->x_integer,
        scroll->x_fraction,
        scroll->y_integer,
        scroll->y_fraction
    );
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_set_enabled(uint8_t enable) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::enable_nbg0(enable != 0);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_palette_upload(const uint16_t* palette_rgb555, uint16_t count, uint16_t offset) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (palette_rgb555 == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    st = validate_vdp2_palette_upload(count, offset);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::upload_palette(palette_rgb555, count, offset);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_vram_write_words(uint32_t word_offset, const uint16_t* words, uint32_t word_count) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (words == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    st = validate_vdp2_vram_write(word_offset, word_count);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::write_vram_words(word_offset, words, word_count);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_map_fill(uint16_t pattern_name) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    const uint32_t map_base_words = compute_map_base_words(g_state.nbg0_map_plane_index);
    const uint32_t map_words = static_cast<uint32_t>(g_state.nbg0_map_width) * static_cast<uint32_t>(g_state.nbg0_map_height);
    saturn::hal::vdp2::fill_vram_words(map_base_words, pattern_name, map_words);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_map_write_region(
    const uint16_t* pattern_names,
    const sat_vdp2_map_region_t* region,
    uint16_t source_stride
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (pattern_names == nullptr || region == nullptr || source_stride == 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    st = validate_map_region(
        region->x, region->y, region->width, region->height,
        g_state.nbg0_map_width, g_state.nbg0_map_height,
        source_stride
    );
    if (st != SAT_OK) {
        return st;
    }

    const uint32_t map_base_words = compute_map_base_words(g_state.nbg0_map_plane_index);
    const uint32_t map_height = g_state.nbg0_map_height;
    uint16_t column_words[64];

    /* JoEngine writes NBG0 map entries column-major.
     * Keep the source buffer row-major for the API, but emit VRAM columns
     * in the same order as the working benchmark.
     */
    for (uint32_t col = 0; col < region->width; ++col) {
        for (uint32_t row = 0; row < region->height; ++row) {
            column_words[row] = pattern_names[(row * static_cast<uint32_t>(source_stride)) + col];
        }

        const uint32_t dst_offset = map_base_words +
            static_cast<uint32_t>(region->x + col) * map_height +
            static_cast<uint32_t>(region->y);
        saturn::hal::vdp2::write_vram_words(dst_offset, column_words, region->height);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_wait_vblank_start(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::wait_vblank_start();
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_wait_vblank_end(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::wait_vblank_end();
    return SAT_OK;
}

/* ================================================================== */
/* RBG0 (Rotation Background 0) API                                   */
/* ================================================================== */

extern "C" sat_result_t sat_vdp2_rbg0_init(const sat_vdp2_rbg0_config_t* config) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    st = validate_rbg0_config(config);
    if (st != SAT_OK) {
        return st;
    }

    /* Save state */
    g_state.rbg0_rot_param_offset = static_cast<uint16_t>(config->rot_param_base_word & 0xFFFFu);

    saturn::hal::vdp2::set_display_enable(false);
    saturn::hal::vdp2::configure_rbg0_bitmap(
        static_cast<saturn::hal::vdp2::RBG0BitmapSize>(config->bitmap_size),
        static_cast<saturn::hal::vdp2::ColorMode>(config->color_mode),
        config->bitmap_base_word,
        config->rot_param_base_word
    );
    saturn::hal::vdp2::enable_rbg0(true);
    saturn::hal::vdp2::set_display_enable(true);

    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_enabled(uint8_t enable) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::enable_rbg0(enable != 0);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_param_mode(sat_vdp2_rbg0_param_mode_t mode) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_param_mode(static_cast<saturn::hal::vdp2::RBG0ParamMode>(mode));
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_rotation_read_control(uint16_t rprctl) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_rotation_read_control(rprctl);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_coefficient_control(uint16_t ktctl) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_coefficient_control(ktctl);
    return SAT_OK;
}

extern "C" uint16_t sat_vdp2_rbg0_last_bgon_written(void) {
    return saturn::hal::vdp2::last_rbg0_bgon_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_ramctl_written(void) {
    return saturn::hal::vdp2::last_rbg0_ramctl_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_chctlb_written(void) {
    return saturn::hal::vdp2::last_rbg0_chctlb_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_mpofr_written(void) {
    return saturn::hal::vdp2::last_rbg0_mpofr_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_rptau_written(void) {
    return saturn::hal::vdp2::last_rbg0_rptau_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_rptal_written(void) {
    return saturn::hal::vdp2::last_rbg0_rptal_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_rprctl_written(void) {
    return saturn::hal::vdp2::last_rbg0_rprctl_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_ktctl_written(void) {
    return saturn::hal::vdp2::last_rbg0_ktctl_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_rpmd_written(void) {
    return saturn::hal::vdp2::last_rbg0_rpmd_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_prir_written(void) {
    return saturn::hal::vdp2::last_rbg0_prir_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_bmpnb_written(void) {
    return saturn::hal::vdp2::last_rbg0_bmpnb_written();
}

extern "C" uint16_t sat_vdp2_rbg0_last_plsz_written(void) {
    return saturn::hal::vdp2::last_rbg0_plsz_written();
}

extern "C" sat_result_t sat_vdp2_rbg0_commit(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::commit_rbg0_config();
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_scroll(uint32_t rot_param_word_offset,
                                                   int32_t xst_int, int32_t xst_frac,
                                                   int32_t yst_int, int32_t yst_frac) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    st = validate_rbg0_rotation_table_offset(rot_param_word_offset);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_scroll(rot_param_word_offset, xst_int, xst_frac, yst_int, yst_frac);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_vertical_increments(uint32_t rot_param_word_offset,
                                                                int32_t dxst_int, int32_t dxst_frac,
                                                                int32_t dyst_int, int32_t dyst_frac) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    st = validate_rbg0_rotation_table_offset(rot_param_word_offset);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_vertical_increments(rot_param_word_offset, dxst_int, dxst_frac, dyst_int, dyst_frac);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_coordinate_increments(uint32_t rot_param_word_offset,
                                                                 int32_t dx_int, int32_t dx_frac,
                                                                 int32_t dy_int, int32_t dy_frac) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    st = validate_rbg0_rotation_table_offset(rot_param_word_offset);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_coordinate_increments(rot_param_word_offset, dx_int, dx_frac, dy_int, dy_frac);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_rotation_matrix(uint32_t rot_param_word_offset,
                                                            int32_t angle_x, int32_t angle_y, int32_t angle_z) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    st = validate_rbg0_rotation_table_offset(rot_param_word_offset);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_rotation_matrix(rot_param_word_offset, angle_x, angle_y, angle_z);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_viewpoint(uint32_t rot_param_word_offset,
                                                      int32_t px, int32_t py, int32_t pz) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    st = validate_rbg0_rotation_table_offset(rot_param_word_offset);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_viewpoint(rot_param_word_offset, px, py, pz);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_center(uint32_t rot_param_word_offset,
                                                   int32_t cx, int32_t cy, int32_t cz) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    st = validate_rbg0_rotation_table_offset(rot_param_word_offset);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_center(rot_param_word_offset, cx, cy, cz);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_scaling(uint32_t rot_param_word_offset,
                                                    int32_t kx, int32_t ky) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    st = validate_rbg0_rotation_table_offset(rot_param_word_offset);
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_scaling(rot_param_word_offset, kx, ky);
    return SAT_OK;
}
