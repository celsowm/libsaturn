#include "saturn/world_lod3d.h"

#include <limits.h>

#include "src/graphics/3d/scene/world_lod_logic.hpp"

namespace saturn::graphics::world_lod {
namespace {

constexpr uint16_t kObjectFlags = SAT_WORLD_LOD3D_OBJECT_OPTIONAL |
                                  SAT_WORLD_LOD3D_OBJECT_PINNED |
                                  SAT_WORLD_LOD3D_OBJECT_ALLOW_EMPTY_FAR;
constexpr uint8_t kLevelFlags = SAT_WORLD_LOD3D_LEVEL_EMPTY;
constexpr uint64_t kNormOneSquared = 0x100000000ull;
constexpr uint64_t kNormMinPercentSquared = 9801ull;
constexpr uint64_t kNormMaxPercentSquared = 10201ull;
constexpr uint32_t kFocalLengthMax = 8192u << 16u;

uint64_t abs64(int64_t value) {
    return value < 0 ? static_cast<uint64_t>(-value) : static_cast<uint64_t>(value);
}

/* Integer square root rounded up without multiplying a potentially large
 * result. The input here is bounded by 3 * 65536^2. */
uint32_t forward_length_ceil(uint64_t norm_squared) {
    uint32_t low = 0u;
    uint32_t high = 131073u;
    while (low + 1u < high) {
        const uint32_t mid = low + (high - low) / 2u;
        if (static_cast<uint64_t>(mid) * mid >= norm_squared) high = mid;
        else low = mid;
    }
    return high;
}

bool valid_view(const sat_world_lod3d_view_t& view) {
    if (view.api_version != SAT_WORLD_LOD3D_API_VERSION ||
        view.struct_size != sizeof(sat_world_lod3d_view_t) ||
        view.viewport_width == 0u || view.viewport_height == 0u ||
        view.focal_length_px_16_16 == 0u ||
        view.focal_length_px_16_16 > kFocalLengthMax ||
        view.near_depth_16_16 == 0u ||
        view.near_depth_16_16 > static_cast<uint32_t>(SAT_WORLD_LOD3D_COORD_LIMIT_FX) ||
        view.plane_count > SAT_WORLD_LOD3D_MAX_PLANES) {
        return false;
    }

    const int32_t forward[3] = {view.forward_x, view.forward_y, view.forward_z};
    uint64_t norm_squared = 0u;
    for (uint8_t i = 0u; i < 3u; ++i) {
        if (forward[i] < -65536 || forward[i] > 65536) return false;
        const int64_t component = forward[i];
        norm_squared += static_cast<uint64_t>(component * component);
    }
    /* Compare norm^2 against [0.99^2, 1.01^2] without floating point. */
    if (norm_squared * 10000ull < kNormOneSquared * kNormMinPercentSquared ||
        norm_squared * 10000ull > kNormOneSquared * kNormMaxPercentSquared) {
        return false;
    }

    for (uint8_t i = 0u; i < view.plane_count; ++i) {
        const sat_world_lod3d_plane_t& plane = view.planes[i];
        const int32_t normal[3] = {plane.normal_x, plane.normal_y, plane.normal_z};
        uint64_t plane_norm_squared = 0u;
        for (uint8_t axis = 0u; axis < 3u; ++axis) {
            if (normal[axis] < -65536 || normal[axis] > 65536) return false;
            const int64_t component = normal[axis];
            plane_norm_squared += static_cast<uint64_t>(component * component);
        }
        if (plane_norm_squared * 10000ull < kNormOneSquared * kNormMinPercentSquared ||
            plane_norm_squared * 10000ull > kNormOneSquared * kNormMaxPercentSquared ||
            abs64(plane.offset) > static_cast<uint32_t>(SAT_WORLD_LOD3D_COORD_LIMIT_FX)) {
            return false;
        }
    }
    return true;
}

bool valid_bounds(const sat_world_lod3d_object_t& object) {
    for (uint8_t axis = 0u; axis < 3u; ++axis) {
        if (object.bounds_min[axis] > object.bounds_max[axis] ||
            abs64(object.bounds_min[axis]) >
                static_cast<uint32_t>(SAT_WORLD_LOD3D_COORD_LIMIT_FX) ||
            abs64(object.bounds_max[axis]) >
                static_cast<uint32_t>(SAT_WORLD_LOD3D_COORD_LIMIT_FX)) {
            return false;
        }
    }
    return true;
}

bool valid_object(const sat_world_lod3d_object_t& object,
                  const sat_world_lod3d_level_t* levels,
                  uint16_t level_count,
                  const sat_world_lod3d_state_t& state) {
    if ((object.flags & ~kObjectFlags) != 0u || object.level_count == 0u ||
        object.level_count > SAT_WORLD_LOD3D_MAX_LEVELS ||
        static_cast<uint64_t>(object.levels_first) + object.level_count > level_count ||
        !valid_bounds(object) || state.valid > 1u) {
        return false;
    }

    bool has_empty = false;
    bool has_nonempty = false;
    uint32_t previous_threshold = UINT32_MAX;
    for (uint8_t i = 0u; i < object.level_count; ++i) {
        const sat_world_lod3d_level_t& level = levels[object.levels_first + i];
        if ((level.flags & ~kLevelFlags) != 0u || level.resident > 1u ||
            (i != 0u && level.min_projected_radius_px_16_16 >= previous_threshold)) {
            return false;
        }
        previous_threshold = level.min_projected_radius_px_16_16;
        if ((level.flags & SAT_WORLD_LOD3D_LEVEL_EMPTY) != 0u) {
            has_empty = true;
            if (level.faces != 0u || level.command_estimate != 0u ||
                level.texture_bytes != 0u ||
                (object.flags & (SAT_WORLD_LOD3D_OBJECT_OPTIONAL |
                                 SAT_WORLD_LOD3D_OBJECT_ALLOW_EMPTY_FAR)) !=
                    (SAT_WORLD_LOD3D_OBJECT_OPTIONAL |
                     SAT_WORLD_LOD3D_OBJECT_ALLOW_EMPTY_FAR)) {
                return false;
            }
        } else {
            if (has_empty) return false;
            has_nonempty = true;
        }
    }
    if (previous_threshold != 0u ||
        (has_empty && (!has_nonempty ||
                       object.empty_max_projected_radius_px_16_16 == 0u))) {
        return false;
    }
    return true;
}

bool less_by_id(const sat_world_lod3d_work_t& a,
                const sat_world_lod3d_work_t& b) {
    return a.stable_id < b.stable_id;
}

bool less_by_priority(const sat_world_lod3d_work_t& a,
                      const sat_world_lod3d_work_t& b,
                      const sat_world_lod3d_object_t* objects) {
    const bool a_candidate = (a.work_flags & kCandidate) != 0u;
    const bool b_candidate = (b.work_flags & kCandidate) != 0u;
    const bool a_prefetch = (a.work_flags & kPrefetch) != 0u;
    const bool b_prefetch = (b.work_flags & kPrefetch) != 0u;
    const bool a_active = a_candidate || a_prefetch;
    const bool b_active = b_candidate || b_prefetch;
    if (a_active != b_active) return a_active;
    if (!a_active) return a.stable_id < b.stable_id;

    const sat_world_lod3d_object_t& ao = objects[a.object_index];
    const sat_world_lod3d_object_t& bo = objects[b.object_index];
    if (ao.priority != bo.priority) return ao.priority < bo.priority;
    if (a.projected_radius_px_16_16 != b.projected_radius_px_16_16)
        return a.projected_radius_px_16_16 < b.projected_radius_px_16_16;
    if (a.camera_depth_16_16 != b.camera_depth_16_16)
        return a.camera_depth_16_16 > b.camera_depth_16_16;
    return a.stable_id < b.stable_id;
}

template <typename Less>
void sift_down(sat_world_lod3d_work_t* items, uint32_t root, uint32_t end,
               Less less) {
    while (root * 2u + 1u < end) {
        uint32_t child = root * 2u + 1u;
        if (child + 1u < end && less(items[child], items[child + 1u])) ++child;
        if (!less(items[root], items[child])) return;
        const sat_world_lod3d_work_t temp = items[root];
        items[root] = items[child];
        items[child] = temp;
        root = child;
    }
}

template <typename Less>
void heap_sort(sat_world_lod3d_work_t* items, uint16_t count, Less less) {
    if (count < 2u) return;
    uint32_t start = count / 2u;
    while (start != 0u) {
        --start;
        sift_down(items, start, count, less);
    }
    uint32_t end = count;
    while (end > 1u) {
        const sat_world_lod3d_work_t temp = items[0];
        items[0] = items[end - 1u];
        items[end - 1u] = temp;
        --end;
        sift_down(items, 0u, end, less);
    }
}

bool outside_frustum(const sat_world_lod3d_view_t& view,
                     const sat_world_lod3d_object_t& object) {
    for (uint8_t i = 0u; i < view.plane_count; ++i) {
        const sat_world_lod3d_plane_t& plane = view.planes[i];
        int64_t center_dot = 0;
        uint64_t support_dot = 0u;
        for (uint8_t axis = 0u; axis < 3u; ++axis) {
            const int64_t min = object.bounds_min[axis];
            const int64_t max = object.bounds_max[axis];
            const int64_t center = (min + max) / 2;
            const uint64_t half_extent = static_cast<uint64_t>(max - min + 1) / 2u;
            const int32_t normal = axis == 0u ? plane.normal_x :
                                   (axis == 1u ? plane.normal_y : plane.normal_z);
            center_dot += static_cast<int64_t>(normal) * center;
            support_dot += static_cast<uint64_t>(abs64(normal)) * half_extent;
        }
        const int64_t offset_raw = static_cast<int64_t>(plane.offset) * 65536ll;
        if (center_dot + offset_raw + static_cast<int64_t>(support_dot) < 0) return true;
    }
    return false;
}

bool projected_bounds(const sat_world_lod3d_view_t& view,
                      const sat_world_lod3d_object_t& object,
                      uint32_t forward_norm_ceil,
                      uint32_t* out_radius, int32_t* out_depth) {
    int64_t center[3] = {};
    uint64_t radius = 0u;
    for (uint8_t axis = 0u; axis < 3u; ++axis) {
        const int64_t min = object.bounds_min[axis];
        const int64_t max = object.bounds_max[axis];
        center[axis] = (min + max) / 2;
        radius += static_cast<uint64_t>(max - min + 1) / 2u;
    }

    const int32_t forward[3] = {view.forward_x, view.forward_y, view.forward_z};
    int64_t dot = 0;
    for (uint8_t axis = 0u; axis < 3u; ++axis) {
        dot += center[axis] * forward[axis];
    }
    const int64_t depth = dot / forward_norm_ceil;
    *out_depth = depth > INT32_MAX ? INT32_MAX :
                 (depth < INT32_MIN ? INT32_MIN : static_cast<int32_t>(depth));
    if (depth + static_cast<int64_t>(radius) < view.near_depth_16_16) return true;

    const uint64_t closest = depth > 0 && static_cast<uint64_t>(depth) > radius
        ? static_cast<uint64_t>(depth) - radius : 0u;
    uint64_t projected = UINT32_MAX;
    if (closest > view.near_depth_16_16) {
        const uint64_t numerator = radius * view.focal_length_px_16_16;
        projected = numerator / closest;
    }
    uint64_t screen_limit = view.viewport_width > view.viewport_height
        ? view.viewport_width : view.viewport_height;
    screen_limit <<= 16u;
    if (projected > screen_limit) projected = screen_limit;
    *out_radius = static_cast<uint32_t>(projected);
    return false;
}

uint8_t requested_lod(const sat_world_lod3d_level_t* levels,
                      const sat_world_lod3d_object_t& object,
                      uint32_t projected_radius) {
    uint8_t desired = static_cast<uint8_t>(object.level_count - 1u);
    for (uint8_t i = 0u; i < object.level_count; ++i) {
        const sat_world_lod3d_level_t& level = levels[object.levels_first + i];
        if (projected_radius >= level.min_projected_radius_px_16_16) {
            desired = i;
            break;
        }
    }
    if ((levels[object.levels_first + desired].flags & SAT_WORLD_LOD3D_LEVEL_EMPTY) != 0u &&
        projected_radius > object.empty_max_projected_radius_px_16_16) {
        while (desired != 0u) {
            --desired;
            if ((levels[object.levels_first + desired].flags &
                 SAT_WORLD_LOD3D_LEVEL_EMPTY) == 0u) break;
        }
    }
    return desired;
}

uint8_t apply_hysteresis(const sat_world_lod3d_view_t& view,
                         const sat_world_lod3d_object_t& object,
                         const sat_world_lod3d_level_t* levels,
                         const sat_world_lod3d_state_t& state,
                         uint32_t projected_radius, uint8_t requested) {
    if (state.valid == 0u || state.object_generation != object.generation ||
        state.view_generation != view.generation ||
        state.selected_lod >= object.level_count) {
        return requested;
    }

    uint8_t lod = state.selected_lod;
    while (lod > requested) {
        const uint8_t finer = static_cast<uint8_t>(lod - 1u);
        const uint64_t promote_at =
            static_cast<uint64_t>(levels[object.levels_first + finer].min_projected_radius_px_16_16) +
            object.lod_hysteresis_px_16_16;
        if (projected_radius < promote_at) break;
        lod = finer;
    }
    while (lod < requested) {
        const uint32_t boundary =
            levels[object.levels_first + lod].min_projected_radius_px_16_16;
        const uint32_t demote_below = boundary > object.lod_hysteresis_px_16_16
            ? boundary - object.lod_hysteresis_px_16_16 : 0u;
        if (projected_radius >= demote_below) break;
        ++lod;
    }
    return lod;
}

bool level_eligible(const sat_world_lod3d_level_t& level,
                    const sat_world_lod3d_object_t& object,
                    uint32_t projected_radius) {
    if (level.resident == 0u || level.slot_generation != level.resident_generation) return false;
    if ((level.flags & SAT_WORLD_LOD3D_LEVEL_EMPTY) != 0u) {
        return projected_radius <= object.empty_max_projected_radius_px_16_16;
    }
    return true;
}

int find_resident_level(const sat_world_lod3d_level_t* levels,
                        const sat_world_lod3d_object_t& object,
                        uint32_t projected_radius, uint8_t desired,
                        bool* out_stale) {
    *out_stale = false;
    for (uint8_t i = 0u; i < object.level_count; ++i) {
        const sat_world_lod3d_level_t& level = levels[object.levels_first + i];
        if (level.resident != 0u && level.slot_generation != level.resident_generation)
            *out_stale = true;
    }
    for (uint8_t i = desired; i < object.level_count; ++i) {
        const sat_world_lod3d_level_t& level = levels[object.levels_first + i];
        if (level_eligible(level, object, projected_radius)) return i;
    }
    for (int i = static_cast<int>(desired) - 1; i >= 0; --i) {
        const sat_world_lod3d_level_t& level = levels[object.levels_first + i];
        if (level_eligible(level, object, projected_radius)) return i;
    }
    return -1;
}

const sat_world_lod3d_level_t& chosen_level(
    const sat_world_lod3d_level_t* levels,
    const sat_world_lod3d_object_t* objects,
    const sat_world_lod3d_work_t& work) {
    const sat_world_lod3d_object_t& object = objects[work.object_index];
    return levels[object.levels_first + work.selected_lod];
}

bool over_budget(const Totals& totals, const sat_world_lod3d_budget_t& budget) {
    return totals.faces > budget.max_faces ||
           totals.commands > budget.max_commands ||
           totals.texture_bytes > budget.max_texture_bytes;
}

bool useful_budget_step(const sat_world_lod3d_level_t& next,
                        const sat_world_lod3d_level_t& current,
                        const Totals& totals,
                        const sat_world_lod3d_budget_t& budget) {
    bool helps = false;
    const uint64_t next_faces = totals.faces - current.faces + next.faces;
    const uint64_t next_commands = totals.commands - current.command_estimate +
                                   next.command_estimate;
    const uint64_t next_textures = totals.texture_bytes - current.texture_bytes +
                                   next.texture_bytes;

    if (totals.faces > budget.max_faces) {
        if (next.faces > current.faces) return false;
        if (next.faces < current.faces) helps = true;
    } else if (next_faces > budget.max_faces) {
        return false;
    }
    if (totals.commands > budget.max_commands) {
        if (next.command_estimate > current.command_estimate) return false;
        if (next.command_estimate < current.command_estimate) helps = true;
    } else if (next_commands > budget.max_commands) {
        return false;
    }
    if (totals.texture_bytes > budget.max_texture_bytes) {
        if (next.texture_bytes > current.texture_bytes) return false;
        if (next.texture_bytes < current.texture_bytes) helps = true;
    } else if (next_textures > budget.max_texture_bytes) {
        return false;
    }
    return helps;
}

void replace_cost(Totals* totals,
                  const sat_world_lod3d_level_t& old_level,
                  const sat_world_lod3d_level_t& new_level) {
    totals->faces -= old_level.faces;
    totals->commands -= old_level.command_estimate;
    totals->texture_bytes -= old_level.texture_bytes;
    totals->faces += new_level.faces;
    totals->commands += new_level.command_estimate;
    totals->texture_bytes += new_level.texture_bytes;
}

int cheaper_resident_lod(const sat_world_lod3d_level_t* levels,
                         const sat_world_lod3d_object_t& object,
                         const sat_world_lod3d_work_t& work,
                         uint8_t max_lod, const Totals& totals,
                         const sat_world_lod3d_budget_t& budget) {
    const sat_world_lod3d_level_t& current = levels[object.levels_first + work.selected_lod];
    for (uint8_t lod = static_cast<uint8_t>(work.selected_lod + 1u);
         lod <= max_lod; ++lod) {
        const sat_world_lod3d_level_t& next = levels[object.levels_first + lod];
        if (level_eligible(next, object, work.projected_radius_px_16_16) &&
            useful_budget_step(next, current, totals, budget)) {
            return lod;
        }
    }
    return -1;
}

void add_cost(Totals* totals, const sat_world_lod3d_level_t& level) {
    totals->faces += level.faces;
    totals->commands += level.command_estimate;
    totals->texture_bytes += level.texture_bytes;
}

void subtract_cost(Totals* totals, const sat_world_lod3d_level_t& level) {
    totals->faces -= level.faces;
    totals->commands -= level.command_estimate;
    totals->texture_bytes -= level.texture_bytes;
}

bool validate_thresholds_fit_viewport(const sat_world_lod3d_view_t& view,
                                      const sat_world_lod3d_object_t& object,
                                      const sat_world_lod3d_level_t* levels) {
    const uint32_t screen_limit =
        static_cast<uint32_t>(view.viewport_width > view.viewport_height
            ? view.viewport_width : view.viewport_height) << 16u;
    for (uint8_t i = 0u; i < object.level_count; ++i) {
        if (levels[object.levels_first + i].min_projected_radius_px_16_16 > screen_limit)
            return false;
    }
    return true;
}

}  // namespace

sat_result_t plan(
    const sat_world_lod3d_view_t* view,
    const sat_world_lod3d_budget_t* budget,
    const sat_world_lod3d_object_t* objects, uint16_t object_count,
    const sat_world_lod3d_level_t* levels, uint16_t level_count,
    sat_world_lod3d_state_t* states,
    sat_world_lod3d_draw_t* draws, uint16_t draw_capacity,
    sat_world_lod3d_request_t* requests, uint16_t request_capacity,
    sat_world_lod3d_work_t* scratch, uint16_t scratch_capacity,
    sat_world_lod3d_stats_t* out_stats) {
    if (view == nullptr || budget == nullptr || out_stats == nullptr ||
        !valid_view(*view) ||
        (object_count != 0u &&
         (objects == nullptr || states == nullptr || scratch == nullptr ||
          scratch_capacity < object_count)) ||
        (draw_capacity != 0u && draws == nullptr) ||
        (request_capacity != 0u && requests == nullptr) ||
        (level_count != 0u && levels == nullptr)) {
        return SAT_ERR_INVALID_ARG;
    }

    sat_world_lod3d_stats_t stats = {};
    stats.considered = object_count;
    if (object_count == 0u) {
        *out_stats = stats;
        return SAT_OK;
    }

    /* Validate every descriptor before changing state or publishing output. */
    for (uint16_t i = 0u; i < object_count; ++i) {
        if (!valid_object(objects[i], levels, level_count, states[i]) ||
            !validate_thresholds_fit_viewport(*view, objects[i], levels)) {
            return SAT_ERR_INVALID_ARG;
        }
        scratch[i] = {};
        scratch[i].stable_id = objects[i].stable_id;
        scratch[i].object_index = i;
        scratch[i].desired_lod = SAT_WORLD_LOD3D_NO_LOD;
        scratch[i].selected_lod = SAT_WORLD_LOD3D_NO_LOD;
    }

    /* Sorting by ID provides permutation-independent duplicate detection and
     * removes caller order from every later tie. */
    heap_sort(scratch, object_count, less_by_id);
    for (uint16_t i = 1u; i < object_count; ++i) {
        if (scratch[i - 1u].stable_id == scratch[i].stable_id) return SAT_ERR_INVALID_ARG;
    }

    const int32_t forward[3] = {view->forward_x, view->forward_y, view->forward_z};
    uint64_t forward_norm_squared = 0u;
    for (uint8_t axis = 0u; axis < 3u; ++axis) {
        const int64_t component = forward[axis];
        forward_norm_squared += static_cast<uint64_t>(component * component);
    }
    const uint32_t forward_norm_ceil = forward_length_ceil(forward_norm_squared);
    for (uint16_t i = 0u; i < object_count; ++i) {
        sat_world_lod3d_work_t& work = scratch[i];
        const sat_world_lod3d_object_t& object = objects[work.object_index];
        const sat_world_lod3d_state_t& state = states[work.object_index];
        if (outside_frustum(*view, object)) {
            ++stats.frustum_rejected;
            continue;
        }
        if (projected_bounds(*view, object, forward_norm_ceil,
                             &work.projected_radius_px_16_16,
                             &work.camera_depth_16_16)) {
            ++stats.frustum_rejected;
            continue;
        }
        work.work_flags |= kVisible;
        const uint8_t raw_lod = requested_lod(
            levels, object, work.projected_radius_px_16_16);
        const uint8_t hysteresis_lod = apply_hysteresis(
            *view, object, levels, state, work.projected_radius_px_16_16, raw_lod);
        work.desired_lod = raw_lod;
        work.reserved[0] = hysteresis_lod;
        ++stats.desired_by_lod[raw_lod];

        bool stale = false;
        const int selected = find_resident_level(
            levels, object, work.projected_radius_px_16_16, hysteresis_lod, &stale);
        if (stale) ++stats.stale_residency_objects;
        if (selected < 0) {
            ++stats.no_resident_lod;
            if ((levels[object.levels_first + hysteresis_lod].flags &
                 SAT_WORLD_LOD3D_LEVEL_EMPTY) == 0u) {
                work.work_flags |= kPrefetch;
            }
            continue;
        }

        work.selected_lod = static_cast<uint8_t>(selected);
        work.work_flags |= kCandidate;
        if (work.selected_lod != hysteresis_lod) {
            work.work_flags |= kResidencyFallback;
            ++stats.residency_fallbacks;
        }
        if (work.selected_lod != hysteresis_lod) work.work_flags |= kPrefetch;
        if ((object.flags & SAT_WORLD_LOD3D_OBJECT_PINNED) != 0u) {
            work.work_flags |= kPinned;
            ++stats.pinned_selected;
        }
        if ((levels[object.levels_first + work.selected_lod].flags &
             SAT_WORLD_LOD3D_LEVEL_EMPTY) != 0u) {
            work.work_flags |= kEmpty;
            ++stats.empty_selected;
        }
    }

    heap_sort(scratch, object_count,
              [objects](const sat_world_lod3d_work_t& a,
                        const sat_world_lod3d_work_t& b) {
                  return less_by_priority(a, b, objects);
              });

    Totals totals = {};
    for (uint16_t i = 0u; i < object_count; ++i) {
        if ((scratch[i].work_flags & kCandidate) != 0u)
            add_cost(&totals, chosen_level(levels, objects, scratch[i]));
    }

    /* Degrade far/low-priority candidates once in deterministic order. Recheck
     * all three actual totals for each candidate step; face, command and
     * texture costs may be non-monotonic and are independent budgets. */
    for (uint16_t i = 0u; i < object_count && over_budget(totals, *budget); ++i) {
        sat_world_lod3d_work_t& work = scratch[i];
        if ((work.work_flags & kCandidate) == 0u) continue;
        const sat_world_lod3d_object_t& object = objects[work.object_index];
        if ((work.work_flags & kPinned) != 0u) continue;
        const uint16_t max_index = static_cast<uint16_t>(work.selected_lod) +
                                   budget->max_budget_lod_loss;
        const uint8_t max_lod = max_index >= object.level_count
            ? static_cast<uint8_t>(object.level_count - 1u)
            : static_cast<uint8_t>(max_index);
        while (over_budget(totals, *budget) && work.selected_lod < max_lod) {
            const int next_lod = cheaper_resident_lod(
                levels, object, work, max_lod, totals, *budget);
            if (next_lod < 0) break;
            const sat_world_lod3d_level_t& old_level = chosen_level(levels, objects, work);
            const sat_world_lod3d_level_t& next_level =
                levels[object.levels_first + static_cast<uint8_t>(next_lod)];
            replace_cost(&totals, old_level, next_level);
            work.selected_lod = static_cast<uint8_t>(next_lod);
            work.work_flags |= kBudgetDegraded;
            if ((next_level.flags & SAT_WORLD_LOD3D_LEVEL_EMPTY) != 0u) {
                work.work_flags |= kEmpty;
                ++stats.empty_selected;
            }
        }
    }

    /* Only explicitly optional, unpinned objects may be dropped. */
    for (uint16_t i = 0u; i < object_count && over_budget(totals, *budget); ++i) {
        sat_world_lod3d_work_t& work = scratch[i];
        if ((work.work_flags & kCandidate) == 0u) continue;
        const sat_world_lod3d_object_t& object = objects[work.object_index];
        if ((object.flags & SAT_WORLD_LOD3D_OBJECT_OPTIONAL) == 0u ||
            (work.work_flags & kPinned) != 0u || (work.work_flags & kEmpty) != 0u) {
            continue;
        }
        const sat_world_lod3d_level_t& dropped_level =
            chosen_level(levels, objects, work);
        if (dropped_level.faces == 0u && dropped_level.command_estimate == 0u &&
            dropped_level.texture_bytes == 0u) continue;
        subtract_cost(&totals, dropped_level);
        work.work_flags |= kDropped;
        work.work_flags &= static_cast<uint8_t>(~kPrefetch);
    }
    if (over_budget(totals, *budget)) return SAT_ERR_CAPACITY;

    uint16_t prefetch_count = 0u;
    for (uint16_t i = 0u; i < object_count; ++i)
        if ((scratch[i].work_flags & kPrefetch) != 0u) ++prefetch_count;

    uint16_t draw_count = 0u;
    for (uint16_t i = 0u; i < object_count; ++i) {
        const sat_world_lod3d_work_t& work = scratch[i];
        if ((work.work_flags & kCandidate) != 0u &&
            (work.work_flags & (kDropped | kEmpty)) == 0u) ++draw_count;
    }
    if (draw_count > draw_capacity) return SAT_ERR_CAPACITY;
    if (prefetch_count > request_capacity) return SAT_ERR_CAPACITY;

    stats.draw_count = draw_count;
    stats.prefetch_requests = prefetch_count;
    stats.faces = static_cast<uint32_t>(totals.faces);
    stats.commands = static_cast<uint32_t>(totals.commands);
    stats.texture_bytes = static_cast<uint32_t>(totals.texture_bytes);
    uint16_t output = 0u;
    for (uint16_t i = 0u; i < object_count; ++i) {
        sat_world_lod3d_work_t& work = scratch[i];
        if ((work.work_flags & kCandidate) == 0u) continue;
        if ((work.work_flags & kDropped) != 0u) {
            ++stats.optional_dropped;
            continue;
        }
        ++stats.selected_by_lod[work.selected_lod];
        if ((work.work_flags & kBudgetDegraded) != 0u)
            ++stats.budget_degraded_objects;
        if ((work.work_flags & kEmpty) != 0u) continue;
        const sat_world_lod3d_object_t& object = objects[work.object_index];
        const sat_world_lod3d_level_t& level = chosen_level(levels, objects, work);
        sat_world_lod3d_draw_t draw = {};
        draw.stable_id = object.stable_id;
        draw.object_generation = object.generation;
        draw.asset_id = level.asset_id;
        draw.residency_generation = level.resident_generation;
        draw.projected_radius_px_16_16 = work.projected_radius_px_16_16;
        draw.camera_depth_16_16 = work.camera_depth_16_16;
        draw.faces = level.faces;
        draw.command_estimate = level.command_estimate;
        draw.texture_bytes = level.texture_bytes;
        draw.desired_lod = work.desired_lod;
        draw.lod = work.selected_lod;
        if ((work.work_flags & kResidencyFallback) != 0u)
            draw.flags |= SAT_WORLD_LOD3D_DRAW_RESIDENCY_FALLBACK;
        if ((work.work_flags & kBudgetDegraded) != 0u) {
            draw.flags |= SAT_WORLD_LOD3D_DRAW_BUDGET_DEGRADED;
        }
        draws[output++] = draw;
    }

    output = 0u;
    for (uint16_t i = 0u; i < object_count; ++i) {
        const sat_world_lod3d_work_t& work = scratch[i];
        if ((work.work_flags & kPrefetch) == 0u) continue;
        const sat_world_lod3d_object_t& object = objects[work.object_index];
        const uint8_t lod = work.reserved[0];
        const sat_world_lod3d_level_t& level = levels[object.levels_first + lod];
        sat_world_lod3d_request_t request = {};
        request.stable_id = object.stable_id;
        request.object_generation = object.generation;
        request.asset_id = level.asset_id;
        request.projected_radius_px_16_16 = work.projected_radius_px_16_16;
        request.lod = lod;
        request.priority = object.priority;
        requests[output++] = request;
    }

    /* Commit temporal state only after every validation, budget and output
     * capacity check has succeeded. Culled, absent and budget-dropped objects
     * lose their prior level so they cannot re-enter with stale hysteresis. */
    for (uint16_t i = 0u; i < object_count; ++i) {
        const sat_world_lod3d_work_t& work = scratch[i];
        const sat_world_lod3d_object_t& object = objects[work.object_index];
        sat_world_lod3d_state_t next = {};
        next.object_generation = object.generation;
        next.view_generation = view->generation;
        if ((work.work_flags & kCandidate) != 0u &&
            (work.work_flags & kDropped) == 0u) {
            next.selected_lod = work.selected_lod;
            next.valid = 1u;
        }
        states[work.object_index] = next;
    }
    *out_stats = stats;
    return SAT_OK;
}

}  // namespace saturn::graphics::world_lod

extern "C" sat_result_t sat_world_lod3d_plan(
    const sat_world_lod3d_view_t* view,
    const sat_world_lod3d_budget_t* budget,
    const sat_world_lod3d_object_t* objects, uint16_t object_count,
    const sat_world_lod3d_level_t* levels, uint16_t level_count,
    sat_world_lod3d_state_t* states,
    sat_world_lod3d_draw_t* draws, uint16_t draw_capacity,
    sat_world_lod3d_request_t* requests, uint16_t request_capacity,
    sat_world_lod3d_work_t* scratch, uint16_t scratch_capacity,
    sat_world_lod3d_stats_t* out_stats) {
    return saturn::graphics::world_lod::plan(
        view, budget, objects, object_count, levels, level_count, states,
        draws, draw_capacity, requests, request_capacity, scratch,
        scratch_capacity, out_stats);
}
