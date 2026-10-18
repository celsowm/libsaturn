#include "saturn/video.h"

#include "src/graphics/2d/palette/tint.hpp"
#include "src/graphics/2d/rendering/runtime.hpp"
#include "src/core/runtime/state.hpp"
#include "src/hal/scu/scu.hpp"
#include "src/hal/vdp1/vdp1.hpp"
#include "src/hal/vdp2/color_calc.hpp"
#include "src/hal/vdp2/vdp2.hpp"
#include "src/hal/video_mode_logic.hpp"

extern "C" sat_result_t sat_begin_frame(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp1::begin_frame(g_state.command_buffer, saturn::internal::kCmdCapacity);
    saturn::hal::vdp2_color_calc::begin_frame();
    tint_cache_begin_frame(g_tint_cache);
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

extern "C" uint16_t sat_video_width(void) {
    return saturn::core::g_state.config.width;
}

extern "C" uint16_t sat_video_height(void) {
    return saturn::core::g_state.config.height;
}

extern "C" uint8_t sat_video_is_hires(void) {
    return saturn::core::g_state.config.width >= 640u ? 1u : 0u;
}

extern "C" sat_result_t sat_video_mode_supported(const sat_video_mode_t* mode) {
    if (mode == nullptr) return SAT_ERR_INVALID_ARG;
    const bool pal = sat_video_is_pal() != 0u;
    return saturn::hal::video_mode::validate(mode->width, mode->height,
                                             mode->interlace, pal);
}

extern "C" uint8_t sat_video_interlace_mode(void) {
    return saturn::core::g_state.config.interlace;
}

extern "C" uint8_t sat_video_is_interlaced(void) {
    return saturn::core::g_state.config.interlace != SAT_VIDEO_INTERLACE_NONE ? 1u : 0u;
}

extern "C" uint16_t sat_video_tvmd(void) {
    return saturn::hal::vdp2::read_tvmd();
}

extern "C" uint16_t sat_video_tvstat(void) {
    return saturn::hal::vdp2::read_tvstat();
}

extern "C" uint8_t sat_video_is_pal(void) {
    return (saturn::hal::vdp2::read_tvstat() & 0x0001u) != 0u ? 1u : 0u;
}

extern "C" uint8_t sat_video_is_ntsc_timing(void) {
    return saturn::core::g_state.config.ntsc != 0u ? 1u : 0u;
}

extern "C" uint32_t sat_video_refresh_hz(void) {
    return saturn::core::g_state.config.ntsc != 0u ? 60u : 50u;
}

extern "C" sat_result_t sat_wait_vblank(void) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::scu::wait_vblank();
    saturn::hal::vdp1::wait_swap_requested();
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

extern "C" sat_result_t sat_vdp1_set_frame_mode(sat_vdp1_frame_mode_t mode) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (mode != SAT_VDP1_FRAME_AUTO && mode != SAT_VDP1_FRAME_ON_SUBMIT) {
        return SAT_ERR_INVALID_ARG;
    }
    saturn::hal::vdp1::set_frame_mode(mode == SAT_VDP1_FRAME_ON_SUBMIT);
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
