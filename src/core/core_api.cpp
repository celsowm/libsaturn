#include "saturn/core.h"

#include "src/core/internal.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/scu.hpp"
#include "src/hal/vdp1.hpp"
#include "src/hal/vdp2.hpp"

extern "C" sat_result_t sat_init(const sat_video_config_t* config) {
    using namespace saturn;
    using namespace saturn::core;

    /* -------------------------------------------------------------------
     * Early hardware initialization
     * -------------------------------------------------------------------
     * PROBLEM: Saturn's BIOS does NOT initialize VDP1/VDP2 completely.
     * If we call saturn::hal::vdp2::init_ntsc_320x224() directly, the hardware may
     * be in undefined state causing visual glitch or crash.
     *
     * SOLUTION: _saturn_early_init() prepares hardware to a known state
     * before any engine-specific configuration.
     *
     * Comparison with other engines:
     *   - libyaul: does this in ___sys_init() (called by crt0)
     *   - JoEngine: does this in jo_core_init() (called by main)
     *   - libsaturn: does this here, at the start of sat_init()
     * ------------------------------------------------------------------- */
    extern void _saturn_early_init(void);
    _saturn_early_init();

    /* Validate parameters */
    if (config == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->width != saturn::internal::kDefaultWidth || config->height != saturn::internal::kDefaultHeight) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (config->ntsc == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }

    /* Initialize runtime state */
    g_state.config = *config;
    g_state.pad = {0, 0, 0};
    g_state.clear_color = 0x0000;
    g_state.nbg0_map_plane_index = 0x0010u;
    g_state.nbg0_map_width = 64u;
    g_state.nbg0_map_height = 64u;
    g_state.initialized = true;

    /* -------------------------------------------------------------------
     * Complete hardware initialization
     * -------------------------------------------------------------------
     * Now that hardware is in clean state (_saturn_early_init),
     * we configure each subsystem with desired parameters.
     *
     * Init order:
     *   1. VDP2 (backgrounds, scroll, VRAM)
     *   2. VDP1 (sprites, polygons, frame buffer)
     *   3. SCU (interrupts, VBlank counter)
     *   4. VDP1 begin_frame (prepare command buffer)
     * ------------------------------------------------------------------- */
    saturn::hal::vdp2::init_ntsc_320x224();
    saturn::hal::vdp1::init(config->width, config->height, g_state.clear_color);
    saturn::hal::scu::init_interrupts();
    saturn::hal::vdp1::begin_frame(g_state.command_buffer, saturn::internal::kCmdCapacity);
    return SAT_OK;
}

extern "C" sat_result_t sat_shutdown(void) {
    using namespace saturn::core;
    g_state.initialized = false;
    return SAT_OK;
}
