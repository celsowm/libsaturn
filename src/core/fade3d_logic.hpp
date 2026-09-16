#ifndef SATURN_CORE_FADE3D_LOGIC_HPP
#define SATURN_CORE_FADE3D_LOGIC_HPP

#include <stdint.h>

#include "saturn/fade3d.h"

namespace saturn::core::fade3d {

inline sat_result_t eval(
    const sat_fade3d_t* fade,
    sat_fx16_t view_depth,
    sat_fade3d_result_t* out
) {
    if (fade == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (fade->start >= fade->end || fade->levels == 0u ||
        (fade->flags & static_cast<uint8_t>(~SAT_FADE3D_CULL_AFTER_END)) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    out->level = 0u;
    out->culled = 0u;
    out->reserved = 0u;

    if (view_depth <= fade->start) {
        return SAT_OK;
    }

    if (view_depth >= fade->end) {
        out->level = static_cast<uint8_t>(fade->levels - 1u);
        out->culled = (fade->flags & SAT_FADE3D_CULL_AFTER_END) != 0u ? 1u : 0u;
        return SAT_OK;
    }

    const uint64_t span = static_cast<uint64_t>(
        static_cast<int64_t>(fade->end) - static_cast<int64_t>(fade->start));
    const uint64_t pos = static_cast<uint64_t>(
        static_cast<int64_t>(view_depth) - static_cast<int64_t>(fade->start));
    uint32_t level = static_cast<uint32_t>(
        (pos * static_cast<uint64_t>(fade->levels)) / span);
    if (level >= fade->levels) {
        level = static_cast<uint32_t>(fade->levels - 1u);
    }
    out->level = static_cast<uint8_t>(level);
    return SAT_OK;
}

}  // namespace saturn::core::fade3d

#endif /* SATURN_CORE_FADE3D_LOGIC_HPP */
