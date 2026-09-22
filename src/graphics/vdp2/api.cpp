#include "saturn/vdp2.h"

#include "src/core/runtime/logic.hpp"
#include "src/graphics/2d/palette/registry.hpp"
#include "src/core/runtime/state.hpp"
#include "src/hal/vdp2/vdp2.hpp"

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

    saturn::hal::vdp2::set_display_enable(false);
    saturn::hal::vdp2::configure_nbg0_character(
        static_cast<saturn::hal::vdp2::CharacterSize>(config->char_size),
        static_cast<saturn::hal::vdp2::ColorMode>(config->color_mode));
    saturn::hal::vdp2::set_nbg0_map_plane_index(config->map_plane_index);
    saturn::hal::vdp2::configure_nbg0_text_layout();
    saturn::hal::vdp2::set_nbg0_transparent_code_enabled(config->transparent_code_enabled != 0u);
    saturn::hal::vdp2::enable_nbg0(true);
    saturn::hal::vdp2::set_display_enable(true);

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

extern "C" sat_result_t sat_vdp2_nbg0_set_priority(uint8_t priority) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp2::set_nbg0_priority(priority);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_sprite_set_priority(uint8_t priority) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp2::set_sprite_priority(priority);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_upload_indexed8(
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t palette_id,
    uint16_t* map_scratch
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (pixels == nullptr || map_scratch == nullptr || palette_id > 7u) {
        return SAT_ERR_INVALID_ARG;
    }

    const uint16_t plane_index = g_state.nbg0_map_plane_index;
    st = validate_nbg0_image(width, height, plane_index);
    if (st != SAT_OK) {
        return st;
    }

    const uint16_t tiles_x = static_cast<uint16_t>(width / kVdp2CellPx);
    const uint16_t tiles_y = static_cast<uint16_t>(height / kVdp2CellPx);
    uint32_t word_offset = kVdp2CellWindowWordBase;
    uint8_t cell[kVdp2CellBytes];
    uint16_t words[kVdp2CellWords];

    for (uint16_t ty = 0; ty < tiles_y; ++ty) {
        for (uint16_t tx = 0; tx < tiles_x; ++tx) {
            build_cell_indexed8(pixels, width, tx, ty, cell);
            /* Packed big-endian into 16-bit words: the VDP2 reads character
             * data as a byte stream, but VRAM is written a word at a time. */
            for (uint16_t i = 0; i < kVdp2CellWords; ++i) {
                words[i] = static_cast<uint16_t>(
                    (static_cast<uint16_t>(cell[i * 2u]) << 8u) |
                    static_cast<uint16_t>(cell[(i * 2u) + 1u]));
            }
            saturn::hal::vdp2::write_vram_words(word_offset, words, kVdp2CellWords);
            word_offset += kVdp2CellWords;
        }
    }

    /* The map covers the whole plane whatever the image size: the source is
     * repeated by wrapping the tile coordinates, which is why a seamless
     * texture tiles cleanly and a non-seamless one shows its seams. */
    for (uint16_t my = 0; my < kVdp2MapCells; ++my) {
        for (uint16_t mx = 0; mx < kVdp2MapCells; ++mx) {
            map_scratch[(my * kVdp2MapCells) + mx] = compose_pattern_name(
                palette_id,
                tiles_x,
                static_cast<uint16_t>(mx % tiles_x),
                static_cast<uint16_t>(my % tiles_y));
        }
    }

    /* The VDP2 reads the page row-major. A column-major write (as JoEngine
     * does) transposes the map: a non-symmetric image such as a sky panorama
     * turns into vertical strips, while a checkerboard hides it completely. */
    saturn::hal::vdp2::write_vram_words(
        nbg0_map_word_base(plane_index),
        map_scratch,
        static_cast<uint32_t>(kVdp2MapCells) * kVdp2MapCells);

    /* In 1-word, 1x1, 256-colour mode the palette bank is carried by pattern
     * name bits 14..12.  PNCN0 supplementary palette bits are not part of the
     * colour address in this mode, so do not mutate them here. */
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
    st = palette_claim_external(g_palette_registry, offset, count);
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

extern "C" sat_result_t sat_vdp2_set_backdrop_color(uint16_t rgb555) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_backdrop_color(rgb555);
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

extern "C" sat_result_t sat_vdp2_rbg0_set_ktaof(uint16_t ktaof) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_ktaof(ktaof);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_priority(uint8_t priority) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_priority(priority);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_rbg0_set_sprite_priority(uint8_t priority) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    saturn::hal::vdp2::set_rbg0_sprite_priority(priority);
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

extern "C" sat_result_t sat_vdp2_rbg0_set_transparent_code_enabled(uint8_t enable) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    saturn::hal::vdp2::set_rbg0_transparent_code_enabled(enable != 0u);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_layers_commit(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    saturn::hal::vdp2::commit_layers();
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

extern "C" sat_result_t sat_vdp2_rbg0_mode7_init(const sat_vdp2_rbg0_mode7_config_t* config) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    sat_vdp2_rbg0_config_t rbg0_cfg = {};
    rbg0_cfg.bitmap_size = config->bitmap_size;
    rbg0_cfg.color_mode = config->color_mode;
    rbg0_cfg.bitmap_base_word = config->bitmap_base_word;
    rbg0_cfg.rot_param_base_word = config->rot_param_base_word;

    st = sat_vdp2_rbg0_init(&rbg0_cfg);
    if (st != SAT_OK) {
        return st;
    }

    /* The Mode-7 coefficient table marks every sky scanline with bit 15.
     * That bit only punches a hole through RBG0 when R0TPON is clear; the
     * generic RBG0 enable path currently leaves R0TPON set, which makes those
     * scanlines opaque black and hides NBG0 completely. */
    SAT_TRY(sat_vdp2_rbg0_set_transparent_code_enabled(1u));

    /* 2-word coefficient table, parameter A, KMD=0 (k applied to both kx and ky) */
    SAT_TRY(sat_vdp2_rbg0_set_coefficient_control(0x0001u));
    SAT_TRY(sat_vdp2_rbg0_set_ktaof(0x0000u));
    SAT_TRY(sat_vdp2_rbg0_set_priority(config->rbg0_priority));
    SAT_TRY(sat_vdp2_rbg0_set_sprite_priority(config->sprite_priority));
    SAT_TRY(sat_vdp2_set_backdrop_color(config->back_color_rgb555));

    /* Make a combined NBG0 + RBG0 setup visible immediately.  Callers still
     * commit once per VBlank for dynamic scroll/rotation state, but the first
     * displayed frame must not depend on reaching that loop first. */
    SAT_TRY(sat_vdp2_layers_commit());

    return SAT_OK;
}
