#include "saturn/core.h"

#include "src/core/internal.hpp"
#include "src/core/palette_registry.hpp"
#include "src/core/render2d_runtime.hpp"
#include "src/core/runtime_state.hpp"
#include "src/core/texture_runtime.hpp"
#include "src/hal/scu.hpp"
#include "src/hal/vdp1.hpp"
#include "src/hal/vdp2.hpp"

extern "C" sat_result_t sat_init(const sat_video_config_t* config) {
    using namespace saturn;
    using namespace saturn::core;

    extern void _saturn_early_init(void);
    _saturn_early_init();

    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->width != saturn::internal::kDefaultWidth || config->height != saturn::internal::kDefaultHeight) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (config->ntsc == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }

    palette_registry_reset(g_palette_registry);
    texture_registry_reset(g_texture_registry);
    render2d_runtime_reset(g_render2d_runtime);

    g_state.config = *config;
    g_state.pad = {0, 0, 0};
    g_state.clear_color = 0x0000;
    g_state.nbg0_map_plane_index = 0x003Bu;
    g_state.nbg0_map_width = 64u;
    g_state.nbg0_map_height = 64u;
    g_state.initialized = true;

    saturn::hal::vdp2::init_ntsc_320x224();
    saturn::hal::vdp1::init(config->width, config->height, g_state.clear_color);
    saturn::hal::scu::init_interrupts();
    saturn::hal::scu::init_frame_clock();
    saturn::hal::vdp1::begin_frame(g_state.command_buffer, saturn::internal::kCmdCapacity);
    return SAT_OK;
}

extern "C" sat_result_t sat_shutdown(void) {
    using namespace saturn::core;
    render2d_runtime_reset(g_render2d_runtime);
    g_state.initialized = false;
    return SAT_OK;
}
