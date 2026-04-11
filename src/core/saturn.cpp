#include "saturn/saturn.h"

#include "src/core/internal.hpp"
#include "src/hal/scu.hpp"
#include "src/hal/smpc.hpp"
#include "src/hal/vdp1.hpp"
#include "src/hal/vdp2.hpp"

namespace saturn::core {

struct RuntimeState {
    bool initialized;
    sat_video_config_t config;
    sat_pad_state_t pad;
    uint16_t clear_color;
    uint16_t nbg0_map_plane_index;
    uint16_t nbg0_map_width;
    uint16_t nbg0_map_height;
    hal::vdp1::Command command_buffer[internal::kCmdCapacity];
};

RuntimeState g_state = {};

inline sat_result_t require_initialized() {
    if (!g_state.initialized) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    return SAT_OK;
}

}  // namespace saturn::core

extern "C" sat_result_t sat_init(const sat_video_config_t* config) {
    using namespace saturn;
    using namespace saturn::core;

    /* -------------------------------------------------------------------
     * Early hardware initialization
     * -------------------------------------------------------------------
     * PROBLEMA: O BIOS do Saturn NÃO inicializa VDP1/VDP2 completamente.
     * Se chamarmos hal::vdp2::init_ntsc_320x224() direto, o hardware pode
     * estar em estado indefinido causando glitch visual ou crash.
     *
     * SOLUÇÃO: _saturn_early_init() prepara o hardware para um estado
     * conhecido antes de qualquer configuração específica do engine.
     *
     * Comparação com outros engines:
     *   - libyaul: faz isso em ___sys_init() (chamado pelo crt0)
     *   - JoEngine: faz isso em jo_core_init() (chamado pelo main)
     *   - libsaturn: faz isso aqui, no início de sat_init()
     * ------------------------------------------------------------------- */
    extern void _saturn_early_init(void);
    _saturn_early_init();

    /* Validar parâmetros */
    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->width != internal::kDefaultWidth || config->height != internal::kDefaultHeight) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (config->ntsc == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }

    /* Inicializar estado do runtime */
    g_state.config = *config;
    g_state.pad = {0, 0, 0};
    g_state.clear_color = 0x0000;
    g_state.nbg0_map_plane_index = 0x0010u;
    g_state.nbg0_map_width = 64u;
    g_state.nbg0_map_height = 64u;
    g_state.initialized = true;

    /* -------------------------------------------------------------------
     * Inicialização completa do hardware
     * -------------------------------------------------------------------
     * Agora que o hardware está em estado limpo (_saturn_early_init),
     * configuramos cada subsistema com os parâmetros desejados.
     *
     * Ordem de init:
     *   1. VDP2 (backgrounds, scroll, VRAM)
     *   2. VDP1 (sprites, polígonos, frame buffer)
     *   3. SCU (interrupts, VBlank counter)
     *   4. VDP1 begin_frame (preparar command buffer)
     * ------------------------------------------------------------------- */
    hal::vdp2::init_ntsc_320x224();
    hal::vdp1::init(config->width, config->height, g_state.clear_color);
    hal::scu::init_interrupts();
    hal::vdp1::begin_frame(g_state.command_buffer, internal::kCmdCapacity);
    return SAT_OK;
}

extern "C" sat_result_t sat_shutdown(void) {
    using namespace saturn::core;
    g_state.initialized = false;
    return SAT_OK;
}

extern "C" sat_result_t sat_begin_frame(void) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    hal::vdp1::begin_frame(g_state.command_buffer, internal::kCmdCapacity);
    return SAT_OK;
}

extern "C" sat_result_t sat_end_frame(void) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    hal::vdp1::submit();
    return SAT_OK;
}

extern "C" sat_result_t sat_wait_vblank(void) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    hal::scu::wait_vblank();
    return SAT_OK;
}

extern "C" sat_result_t sat_pad_poll(sat_pad_state_t* out_state) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (out_state == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    const uint16_t prev = g_state.pad.held;
    const uint16_t held = hal::smpc::read_digital_pad();
    g_state.pad.held = held;
    g_state.pad.pressed = static_cast<uint16_t>((~prev) & held);
    g_state.pad.released = static_cast<uint16_t>(prev & (~held));
    *out_state = g_state.pad;
    return SAT_OK;
}

extern "C" uint16_t sat_pad_held(void) {
    using namespace saturn::core;
    return g_state.pad.held;
}

extern "C" sat_result_t sat_tex_upload_indexed8(
    sat_texture_t* out_texture,
    const uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    const uint16_t* palette_rgb555,
    uint16_t palette_index
) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (out_texture == nullptr || pixels == nullptr || palette_rgb555 == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }

    st = hal::vdp1::upload_palette(palette_rgb555, palette_index);
    if (st != SAT_OK) {
        return st;
    }

    uint16_t srca = 0;
    st = hal::vdp1::upload_texture_indexed8(pixels, width, height, &srca);
    if (st != SAT_OK) {
        return st;
    }

    out_texture->srca = srca;
    out_texture->width = width;
    out_texture->height = height;
    out_texture->palette = palette_index;
    out_texture->valid = 1;
    out_texture->reserved = 0;
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_sprite(const sat_sprite_cmd_t* cmd) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (cmd == nullptr || cmd->texture == nullptr || cmd->texture->valid == 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    hal::vdp1::SpriteRequest req = {};
    req.x = internal::fx16_to_int(cmd->x);
    req.y = internal::fx16_to_int(cmd->y);
    req.width = (cmd->width != 0u) ? cmd->width : cmd->texture->width;
    req.height = (cmd->height != 0u) ? cmd->height : cmd->texture->height;
    req.srca = cmd->texture->srca;
    req.palette = (cmd->palette_override != 0u) ? cmd->palette_override : cmd->texture->palette;
    return hal::vdp1::push_sprite(req);
}

extern "C" sat_result_t sat_set_clear_color(uint16_t rgb555) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    g_state.clear_color = rgb555;
    hal::vdp1::set_clear_color(rgb555);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_init(const sat_vdp2_nbg0_config_t* config) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->char_size > SAT_VDP2_CHAR_SIZE_2X2) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->color_mode > SAT_VDP2_COLOR_MODE_16770000) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->map_plane_index > 0x003Fu) {
        return SAT_ERR_INVALID_ARG;
    }

    hal::vdp2::CharacterSize cs = (config->char_size == SAT_VDP2_CHAR_SIZE_2X2)
        ? hal::vdp2::CHAR_SIZE_2x2
        : hal::vdp2::CHAR_SIZE_1x1;
    hal::vdp2::ColorMode cm = static_cast<hal::vdp2::ColorMode>(config->color_mode);
    hal::vdp2::configure_nbg0_character(cs, cm);
    hal::vdp2::configure_nbg0_text_layout();
    hal::vdp2::set_nbg0_map_plane_index(config->map_plane_index);
    hal::vdp2::set_nbg0_transparent_code_enabled(config->transparent_code_enabled != 0u);
    hal::vdp2::set_nbg0_scroll(0, 0, 0, 0);

    g_state.nbg0_map_plane_index = config->map_plane_index;
    g_state.nbg0_map_width = 64u;
    g_state.nbg0_map_height = 64u;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_set_scroll(const sat_vdp2_scroll_t* scroll) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (scroll == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    hal::vdp2::set_nbg0_scroll(
        scroll->x_integer,
        scroll->x_fraction,
        scroll->y_integer,
        scroll->y_fraction
    );
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_set_enabled(uint8_t enable) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    
    hal::vdp2::enable_nbg0(enable != 0);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_palette_upload(const uint16_t* palette_rgb555, uint16_t count, uint16_t offset) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (palette_rgb555 == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    constexpr uint32_t kVdp2CramWordCapacity = 2048u;
    if ((static_cast<uint32_t>(offset) + static_cast<uint32_t>(count)) > kVdp2CramWordCapacity) {
        return SAT_ERR_CAPACITY;
    }
    
    hal::vdp2::upload_palette(palette_rgb555, count, offset);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_vram_write_words(uint32_t word_offset, const uint16_t* words, uint32_t word_count) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (words == nullptr || word_count == 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    constexpr uint32_t kVdp2VramWordCapacity = (512u * 1024u) / 2u;
    if (word_offset >= kVdp2VramWordCapacity || (word_offset + word_count) > kVdp2VramWordCapacity) {
        return SAT_ERR_CAPACITY;
    }

    hal::vdp2::write_vram_words(word_offset, words, word_count);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_map_fill(uint16_t pattern_name) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }

    const uint32_t map_base_words = static_cast<uint32_t>(g_state.nbg0_map_plane_index) << 10u;
    const uint32_t map_words = static_cast<uint32_t>(g_state.nbg0_map_width) * static_cast<uint32_t>(g_state.nbg0_map_height);
    hal::vdp2::fill_vram_words(map_base_words, pattern_name, map_words);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_nbg0_map_write_region(
    const uint16_t* pattern_names,
    const sat_vdp2_map_region_t* region,
    uint16_t source_stride
) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (pattern_names == nullptr || region == nullptr || source_stride == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (region->width == 0u || region->height == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (region->x >= g_state.nbg0_map_width || region->y >= g_state.nbg0_map_height) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((static_cast<uint32_t>(region->x) + static_cast<uint32_t>(region->width)) > g_state.nbg0_map_width) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((static_cast<uint32_t>(region->y) + static_cast<uint32_t>(region->height)) > g_state.nbg0_map_height) {
        return SAT_ERR_INVALID_ARG;
    }
    if (source_stride < region->width) {
        return SAT_ERR_INVALID_ARG;
    }

    const uint32_t map_base_words = static_cast<uint32_t>(g_state.nbg0_map_plane_index) << 10u;
    const uint32_t map_width = g_state.nbg0_map_width;

    for (uint32_t row = 0; row < region->height; ++row) {
        const uint32_t dst_offset = map_base_words +
            (static_cast<uint32_t>(region->y) + row) * map_width +
            static_cast<uint32_t>(region->x);
        const uint32_t src_offset = row * static_cast<uint32_t>(source_stride);
        hal::vdp2::write_vram_words(dst_offset, pattern_names + src_offset, region->width);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_wait_vblank_start(void) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    
    hal::vdp2::wait_vblank_start();
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_wait_vblank_end(void) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    
    hal::vdp2::wait_vblank_end();
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_back_color_set(uint16_t rgb555) {
    using namespace saturn;
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    
    hal::vdp2::set_backdrop_color(rgb555);
    return SAT_OK;
}
