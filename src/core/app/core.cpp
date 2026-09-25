#include "saturn/core.h"
#include "saturn/audio.h"
#include "saturn/parallel.h"

#include "src/core/runtime/internal.hpp"
#include "src/input/runtime.hpp"
#include "src/graphics/2d/palette/registry.hpp"
#include "src/graphics/2d/palette/tint.hpp"
#include "src/storage/files/asset_runtime.hpp"
#include "src/graphics/2d/rendering/runtime.hpp"
#include "src/core/runtime/state.hpp"
#include "src/graphics/2d/textures/runtime.hpp"
#include "src/hal/scu/scu.hpp"
#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/vdp2/vdp2.hpp"

extern "C" sat_result_t sat_init(const sat_video_config_t* config) {
    using namespace saturn;
    using namespace saturn::core;

    extern void _saturn_early_init(void);
    _saturn_early_init();

    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if ((config->width != saturn::internal::kDefaultWidth &&
         config->width != 640u && config->width != 704u) ||
        config->height != saturn::internal::kDefaultHeight) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (config->ntsc == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }

    input_runtime_reset(g_input_runtime);
    file_asset_runtime_reset(g_file_asset_runtime);
    palette_registry_reset(g_palette_registry);
    tint_cache_reset(g_tint_cache);
    texture_registry_reset(g_texture_registry);
    render2d_runtime_reset(g_render2d_runtime);

    g_state.config = *config;
    g_state.clear_color = 0x0000;
    g_state.nbg0_map_plane_index = 0x003Bu;
    g_state.nbg0_map_width = 64u;
    g_state.nbg0_map_height = 64u;
    g_state.initialized = true;

    saturn::hal::vdp2::init_ntsc_320x224();
    saturn::hal::vdp2::set_horizontal_resolution(config->width);
    saturn::hal::vdp1::init(config->width, config->height, g_state.clear_color);
    saturn::hal::scu::init_interrupts();
    saturn::hal::scu::init_frame_clock();
    saturn::hal::vdp1::begin_frame(g_state.command_buffer, saturn::internal::kCmdCapacity);
    return SAT_OK;
}

extern "C" sat_result_t sat_shutdown(void) {
    using namespace saturn::core;
    SAT_TRY(sat_parallel_shutdown(60000u));
    /* Audio is an optional subsystem, but shutdown owns the complete runtime
     * lifecycle when it is active. sat_audio_shutdown() is idempotent, so
     * applications that already shut audio down explicitly remain valid. */
    SAT_TRY(sat_audio_shutdown());
    input_runtime_reset(g_input_runtime);
    file_asset_runtime_reset(g_file_asset_runtime);
    palette_registry_reset(g_palette_registry);
    tint_cache_reset(g_tint_cache);
    texture_registry_reset(g_texture_registry);
    render2d_runtime_reset(g_render2d_runtime);
    g_state.initialized = false;
    return SAT_OK;
}
