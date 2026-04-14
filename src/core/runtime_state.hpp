#ifndef SATURN_CORE_RUNTIME_STATE_HPP
#define SATURN_CORE_RUNTIME_STATE_HPP

#include "saturn/saturn.h"
#include "src/core/internal.hpp"
#include "src/hal/vdp1.hpp"

namespace saturn::core {

struct RuntimeState {
    bool initialized;
    sat_video_config_t config;
    sat_pad_state_t pad;
    uint16_t clear_color;
    uint16_t nbg0_map_plane_index;
    uint16_t nbg0_map_width;
    uint16_t nbg0_map_height;
    uint16_t rbg0_rot_param_offset;
    saturn::hal::vdp1::Command command_buffer[saturn::internal::kCmdCapacity];
};

extern RuntimeState g_state;

inline sat_result_t require_initialized() {
    if (!g_state.initialized) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    return SAT_OK;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_RUNTIME_STATE_HPP */
