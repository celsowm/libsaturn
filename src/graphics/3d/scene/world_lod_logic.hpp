#ifndef SATURN_GRAPHICS_3D_SCENE_WORLD_LOD_LOGIC_HPP
#define SATURN_GRAPHICS_3D_SCENE_WORLD_LOD_LOGIC_HPP

#include <stdint.h>

#include "saturn/world_lod3d.h"

namespace saturn::graphics::world_lod {

enum WorkFlags : uint8_t {
    kVisible = 1u << 0,
    kCandidate = 1u << 1,
    kResidencyFallback = 1u << 2,
    kBudgetDegraded = 1u << 3,
    kDropped = 1u << 4,
    kEmpty = 1u << 5,
    kPinned = 1u << 6,
    kPrefetch = 1u << 7,
};

struct Totals {
    uint64_t faces;
    uint64_t commands;
    uint64_t texture_bytes;
};

sat_result_t plan(
    const sat_world_lod3d_view_t* view,
    const sat_world_lod3d_budget_t* budget,
    const sat_world_lod3d_object_t* objects, uint16_t object_count,
    const sat_world_lod3d_level_t* levels, uint16_t level_count,
    sat_world_lod3d_state_t* states,
    sat_world_lod3d_draw_t* draws, uint16_t draw_capacity,
    sat_world_lod3d_request_t* requests, uint16_t request_capacity,
    sat_world_lod3d_work_t* scratch, uint16_t scratch_capacity,
    sat_world_lod3d_stats_t* out_stats);

}  // namespace saturn::graphics::world_lod

#endif
