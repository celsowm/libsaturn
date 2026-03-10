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

    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->width != internal::kDefaultWidth || config->height != internal::kDefaultHeight) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (config->ntsc == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }

    g_state.config = *config;
    g_state.pad = {0, 0, 0};
    g_state.clear_color = 0x0000;
    g_state.initialized = true;

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

