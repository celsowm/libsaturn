#include "saturn/video.h"

#include "src/graphics/2d/rendering/runtime.hpp"
#include "src/core/runtime/state.hpp"
#include "src/hal/scu/scu.hpp"
#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/vdp2/vdp2.hpp"

extern "C" sat_result_t sat_begin_frame(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp1::begin_frame(g_state.command_buffer, saturn::internal::kCmdCapacity);
    g_render2d_runtime.clip_dirty = g_render2d_runtime.current.clip_enabled;
    return render2d_ensure_clip(g_state.config.width, g_state.config.height);
}

extern "C" sat_result_t sat_end_frame(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp1::submit();
    return SAT_OK;
}

extern "C" sat_result_t sat_wait_vblank(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::scu::wait_vblank();
    return SAT_OK;
}

extern "C" uint32_t sat_frame_count(void) {
    return saturn::hal::scu::display_frames();
}

extern "C" sat_result_t sat_set_clear_color(uint16_t rgb555) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    g_state.clear_color = rgb555;
    saturn::hal::vdp1::set_clear_color(rgb555);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp1_set_erase_transparent(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp1::set_erase_transparent();
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_back_color_set(uint16_t rgb555) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp2::set_backdrop_color(rgb555);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp1_set_erase_enabled(uint8_t enable) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp1::set_erase_enabled(enable != 0, g_state.config.width, g_state.config.height);
    return SAT_OK;
}
