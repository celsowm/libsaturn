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

/* Level -> one of the VDP2's eight sprite color-calculation slots. */
inline uint8_t level_slot(const sat_fade3d_slots_t& cfg, uint8_t level) {
    return static_cast<uint8_t>(
        cfg.base_slot + static_cast<uint16_t>(level) * cfg.slot_stride);
}

/* Inverse of level_slot, for recovering the level a remembered slot came
 * from. Returns false for the opaque/culled sentinels and for any slot the
 * mapping cannot produce, in which case there is no band to stay inside. */
inline bool slot_level(
    const sat_fade3d_slots_t& cfg, uint8_t slot, uint8_t* out_level) {
    if (slot<cfg.base_slot || cfg.slot_stride==0u) return false;
    const uint16_t offset=static_cast<uint16_t>(slot-cfg.base_slot);
    if (offset%cfg.slot_stride) return false;
    const uint16_t level=static_cast<uint16_t>(offset/cfg.slot_stride);
    if (level>=cfg.policy.levels) return false;
    *out_level=static_cast<uint8_t>(level);
    return true;
}

inline sat_result_t slot(
    const sat_fade3d_slots_t* cfg,
    sat_fx16_t view_depth,
    uint8_t* inout_state,
    uint8_t* out_slot
) {
    if (cfg == nullptr || out_slot == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (cfg->slot_stride == 0u || cfg->hysteresis < 0) {
        return SAT_ERR_INVALID_ARG;
    }
    /* Every level must land on a real slot; a policy that runs off the end of
     * the eight-slot table is a configuration bug, not a run-time clamp. */
    if (static_cast<uint32_t>(cfg->base_slot) +
        static_cast<uint32_t>(cfg->policy.levels - 1u) *
        static_cast<uint32_t>(cfg->slot_stride) > 7u) {
        return SAT_ERR_INVALID_ARG;
    }

    sat_fade3d_result_t evaluated={};
    const sat_result_t st=eval(&cfg->policy, view_depth, &evaluated);
    if (st != SAT_OK) {
        return st;
    }

    const uint8_t target=evaluated.culled
        ? static_cast<uint8_t>(SAT_FADE3D_SLOT_CULLED)
        : ((cfg->opaque_before_start && view_depth <= cfg->policy.start)
            ? static_cast<uint8_t>(SAT_INDEXED_SOLID_OPAQUE)
            : level_slot(*cfg, evaluated.level));

    if (inout_state == nullptr || cfg->hysteresis == 0) {
        if (inout_state != nullptr) *inout_state=target;
        *out_slot=target;
        return SAT_OK;
    }

    /* Keep the previous slot until the depth has travelled a full band PAST
     * the transition that would change it, so a camera easing across a
     * boundary cannot toggle one object back and forth every frame. */
    const uint8_t previous=*inout_state;
    const sat_fx16_t band=cfg->hysteresis;
    uint8_t level=0u;
    bool sticky=false;
    if (previous==SAT_INDEXED_SOLID_OPAQUE) {
        sticky=view_depth <= cfg->policy.start + band;
    } else if (previous==SAT_FADE3D_SLOT_CULLED) {
        sticky=view_depth >= cfg->policy.end - band;
    } else if (slot_level(*cfg, previous, &level)) {
        /* The span is derived from policy.levels, so changing the level count
         * cannot silently leave the bands measuring the wrong distance. */
        const int64_t span=(static_cast<int64_t>(cfg->policy.end) -
                            static_cast<int64_t>(cfg->policy.start)) /
                           static_cast<int64_t>(cfg->policy.levels);
        const int64_t low=static_cast<int64_t>(cfg->policy.start) +
                          span*level - band;
        const int64_t high=static_cast<int64_t>(cfg->policy.start) +
                           span*(level+1) + band;
        sticky=view_depth >= low && view_depth <= high;
    }
    if (sticky) {
        *out_slot=previous;
        return SAT_OK;
    }
    *inout_state=target;
    *out_slot=target;
    return SAT_OK;
}

}  // namespace saturn::core::fade3d

#endif /* SATURN_CORE_FADE3D_LOGIC_HPP */
