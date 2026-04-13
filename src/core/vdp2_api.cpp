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

    saturn::hal::vdp2::CharacterSize cs = (config->char_size == SAT_VDP2_CHAR_SIZE_2X2)
        ? saturn::hal::vdp2::CHAR_SIZE_2x2
        : saturn::hal::vdp2::CHAR_SIZE_1x1;
    saturn::hal::vdp2::ColorMode cm = static_cast<saturn::hal::vdp2::ColorMode>(config->color_mode);
    saturn::hal::vdp2::configure_nbg0_character(cs, cm);
    saturn::hal::vdp2::configure_nbg0_text_layout();
    saturn::hal::vdp2::set_nbg0_map_plane_index(config->map_plane_index);
    saturn::hal::vdp2::set_nbg0_transparent_code_enabled(config->transparent_code_enabled != 0u);
    saturn::hal::vdp2::set_nbg0_scroll(0, 0, 0, 0);

    g_state.nbg0_map_plane_index = config->map_plane_index;
    g_state.nbg0_map_width = 64u;
    g_state.nbg0_map_height = 64u;
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
    const uint32_t map_width = g_state.nbg0_map_width;

    for (uint32_t row = 0; row < region->height; ++row) {
        const uint32_t dst_offset = map_base_words +
            (static_cast<uint32_t>(region->y) + row) * map_width +
            static_cast<uint32_t>(region->x);
        const uint32_t src_offset = row * static_cast<uint32_t>(source_stride);
        saturn::hal::vdp2::write_vram_words(dst_offset, pattern_names + src_offset, region->width);
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
