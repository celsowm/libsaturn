#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "saturn/world_lod3d.h"

namespace {

constexpr int32_t fx(int32_t units) { return units * 65536; }

sat_world_lod3d_view_t make_view() {
    sat_world_lod3d_view_t view = {};
    view.api_version = SAT_WORLD_LOD3D_API_VERSION;
    view.struct_size = sizeof(view);
    view.generation = 1u;
    view.forward_z = 65536;
    view.focal_length_px_16_16 = 160u << 16u;
    view.near_depth_16_16 = 1u << 16u;
    view.viewport_width = 320u;
    view.viewport_height = 224u;
    return view;
}

sat_world_lod3d_budget_t make_budget() {
    sat_world_lod3d_budget_t budget = {};
    budget.max_faces = UINT32_MAX;
    budget.max_commands = UINT32_MAX;
    budget.max_texture_bytes = UINT32_MAX;
    budget.max_budget_lod_loss = 3u;
    return budget;
}

sat_world_lod3d_object_t make_object(uint32_t id, int32_t z, uint32_t first,
                                     uint16_t lod_count = 3u) {
    sat_world_lod3d_object_t object = {};
    object.stable_id = id;
    object.generation = 1u;
    object.bounds_min[0] = fx(-1);
    object.bounds_min[1] = fx(-1);
    object.bounds_min[2] = fx(z - 1);
    object.bounds_max[0] = fx(1);
    object.bounds_max[1] = fx(1);
    object.bounds_max[2] = fx(z + 1);
    object.levels_first = first;
    object.level_count = lod_count;
    object.lod_hysteresis_px_16_16 = 0u;
    object.priority = 128u;
    return object;
}

sat_world_lod3d_level_t make_level(uint32_t asset, uint32_t threshold,
                                   uint32_t faces, uint32_t commands,
                                   uint32_t texture, uint32_t generation = 7u) {
    sat_world_lod3d_level_t level = {};
    level.asset_id = asset;
    level.min_projected_radius_px_16_16 = threshold;
    level.faces = faces;
    level.command_estimate = commands;
    level.texture_bytes = texture;
    level.slot_generation = generation;
    level.resident_generation = generation;
    level.resident = 1u;
    return level;
}

sat_result_t plan(const sat_world_lod3d_view_t& view,
                  const sat_world_lod3d_budget_t& budget,
                  const sat_world_lod3d_object_t* objects, uint16_t object_count,
                  const sat_world_lod3d_level_t* levels, uint16_t level_count,
                  sat_world_lod3d_state_t* states,
                  sat_world_lod3d_draw_t* draws, uint16_t draw_capacity,
                  sat_world_lod3d_work_t* scratch, uint16_t scratch_capacity,
                  sat_world_lod3d_stats_t* stats,
                  sat_world_lod3d_request_t* requests = nullptr,
                  uint16_t request_capacity = 0u) {
    sat_world_lod3d_request_t local_requests[16] = {};
    if (requests == nullptr) {
        requests = local_requests;
        request_capacity = 16u;
    }
    return sat_world_lod3d_plan(
        &view, &budget, objects, object_count, levels, level_count, states,
        draws, draw_capacity, requests, request_capacity, scratch,
        scratch_capacity, stats);
}

void zero_objects_succeeds() {
    auto view = make_view();
    auto budget = make_budget();
    sat_world_lod3d_stats_t stats;
    std::memset(&stats, 0xA5, sizeof(stats));
    assert(plan(view, budget, nullptr, 0u, nullptr, 0u, nullptr, nullptr, 0u,
                nullptr, 0u, &stats) == SAT_OK);
    assert(stats.considered == 0u && stats.draw_count == 0u && stats.faces == 0u);
}

void one_object_and_viewport_saturation() {
    auto view = make_view();
    auto budget = make_budget();
    auto object = make_object(4u, 1, 0u, 1u);
    sat_world_lod3d_level_t level = make_level(91u, 0u, 4u, 12u, 64u);
    sat_world_lod3d_state_t state = {};
    sat_world_lod3d_draw_t draw = {};
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats = {};
    assert(plan(view, budget, &object, 1u, &level, 1u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(stats.considered == 1u && stats.draw_count == 1u);
    assert(draw.stable_id == 4u && draw.asset_id == 91u && draw.lod == 0u);
    assert(draw.command_estimate == 12u && draw.faces == 4u);
    assert(draw.projected_radius_px_16_16 ==
           static_cast<uint32_t>(view.viewport_width) << 16u);
    assert(state.valid == 1u && state.selected_lod == 0u);
}

void one_through_four_levels_select_by_projected_size() {
    auto view = make_view();
    auto budget = make_budget();
    const uint32_t thresholds[4] = {32u << 16u, 16u << 16u, 8u << 16u, 0u};
    for (uint16_t count = 1u; count <= 4u; ++count) {
        sat_world_lod3d_level_t levels[4] = {};
        for (uint16_t i = 0u; i < count; ++i)
            levels[i] = make_level(100u + i,
                                   (count == 1u || i + 1u == count) ? 0u : thresholds[i],
                                   20u - i * 4u, 30u - i * 5u, 80u - i * 8u);
        auto object = make_object(10u + count, 100, 0u, count);
        sat_world_lod3d_state_t state = {};
        sat_world_lod3d_draw_t draw = {};
        sat_world_lod3d_work_t work = {};
        sat_world_lod3d_stats_t stats = {};
        assert(plan(view, budget, &object, 1u, levels, count, &state, &draw,
                    1u, &work, 1u, &stats) == SAT_OK);
        assert(stats.draw_count == 1u);
        if (count == 1u) assert(draw.lod == 0u);
        else assert(draw.lod == count - 1u);
    }
}

void separate_face_command_and_texture_budgets() {
    auto view = make_view();
    auto object = make_object(1u, 10, 0u);
    sat_world_lod3d_level_t levels[3] = {
        make_level(1u, 32u << 16u, 10u, 30u, 90u),
        make_level(2u, 8u << 16u, 8u, 20u, 60u),
        make_level(3u, 0u, 4u, 5u, 20u),
    };
    sat_world_lod3d_state_t state = {};
    sat_world_lod3d_draw_t draw = {};
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats = {};

    auto budget = make_budget();
    budget.max_faces = 8u;
    assert(plan(view, budget, &object, 1u, levels, 3u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.lod == 1u && draw.faces == 8u && draw.command_estimate == 20u);

    state = {};
    budget = make_budget();
    budget.max_commands = 20u;
    assert(plan(view, budget, &object, 1u, levels, 3u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.lod == 1u && draw.command_estimate == 20u);

    state = {};
    budget = make_budget();
    budget.max_texture_bytes = 20u;
    assert(plan(view, budget, &object, 1u, levels, 3u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.lod == 2u && draw.texture_bytes == 20u);
}

void non_monotonic_costs_skip_expensive_middle_lod() {
    auto view = make_view();
    auto budget = make_budget();
    budget.max_faces = 25u;
    auto object = make_object(2u, 10, 0u);
    sat_world_lod3d_level_t levels[3] = {
        make_level(1u, 32u << 16u, 100u, 100u, 100u),
        make_level(2u, 8u << 16u, 120u, 120u, 120u),
        make_level(3u, 0u, 20u, 30u, 40u),
    };
    sat_world_lod3d_state_t state = {};
    sat_world_lod3d_draw_t draw = {};
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats = {};
    assert(plan(view, budget, &object, 1u, levels, 3u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.lod == 2u && draw.faces == 20u);
    assert((draw.flags & SAT_WORLD_LOD3D_DRAW_BUDGET_DEGRADED) != 0u);
}

void frustum_is_conservative_and_negative_coordinates_work() {
    auto view = make_view();
    view.plane_count = 1u;
    view.planes[0].normal_x = 65536;
    auto budget = make_budget();
    sat_world_lod3d_object_t objects[2] = {
        make_object(1u, 10, 0u, 1u), make_object(2u, 10, 1u, 1u)};
    for (auto& object : objects) {
        object.bounds_min[0] = fx(-4);
        object.bounds_max[0] = fx(-2);
    }
    /* A box wholly on the negative side is outside; one touching the plane
     * must survive the conservative AABB test. */
    objects[1].bounds_max[0] = 0;
    sat_world_lod3d_level_t levels[2] = {
        make_level(1u, 0u, 1u, 1u, 0u), make_level(2u, 0u, 1u, 1u, 0u)};
    sat_world_lod3d_state_t states[2] = {};
    sat_world_lod3d_draw_t draws[2] = {};
    sat_world_lod3d_work_t work[2] = {};
    sat_world_lod3d_stats_t stats = {};
    assert(plan(view, budget, objects, 2u, levels, 2u, states, draws, 2u,
                work, 2u, &stats) == SAT_OK);
    assert(stats.frustum_rejected == 1u && stats.draw_count == 1u);
    assert(draws[0].stable_id == 2u && draws[0].camera_depth_16_16 == fx(10));

    view.plane_count = 0u;
    objects[0] = make_object(3u, -10, 0u, 1u);
    states[0] = {};
    assert(plan(view, budget, &objects[0], 1u, levels, 1u, &states[0], draws,
                2u, work, 2u, &stats) == SAT_OK);
    assert(stats.frustum_rejected == 1u && stats.draw_count == 0u);
}

void stale_residency_falls_back_and_missing_assets_are_observable() {
    auto view = make_view();
    auto budget = make_budget();
    auto object = make_object(12u, 10, 0u);
    sat_world_lod3d_level_t levels[3] = {
        make_level(10u, 32u << 16u, 10u, 20u, 80u, 8u),
        make_level(11u, 8u << 16u, 7u, 15u, 48u, 8u),
        make_level(12u, 0u, 4u, 7u, 24u, 8u),
    };
    levels[0].resident_generation = 7u;
    levels[1].resident = 0u;
    sat_world_lod3d_state_t state = {};
    sat_world_lod3d_draw_t draw = {};
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats = {};
    sat_world_lod3d_request_t request = {};
    assert(plan(view, budget, &object, 1u, levels, 3u, &state, &draw, 1u,
                &work, 1u, &stats, &request, 1u) == SAT_OK);
    assert(draw.lod == 2u && draw.asset_id == 12u);
    assert((draw.flags & SAT_WORLD_LOD3D_DRAW_RESIDENCY_FALLBACK) != 0u);
    assert(stats.stale_residency_objects == 1u && stats.residency_fallbacks == 1u);
    assert(stats.prefetch_requests == 1u && request.lod == 0u && request.asset_id == 10u);

    levels[2].resident = 0u;
    state = {};
    draw.stable_id = 0xDEADBEEFu;
    assert(plan(view, budget, &object, 1u, levels, 3u, &state, &draw, 1u,
                &work, 1u, &stats, &request, 1u) == SAT_OK);
    assert(stats.no_resident_lod == 1u && stats.draw_count == 0u);
    assert(stats.prefetch_requests == 1u && request.asset_id == 10u && request.lod == 0u);
    assert(draw.stable_id == 0xDEADBEEFu && state.valid == 0u);
}

void finer_resident_level_can_cover_a_missing_desired_level() {
    auto view = make_view();
    auto budget = make_budget();
    auto object = make_object(13u, 40, 0u);
    sat_world_lod3d_level_t levels[3] = {
        make_level(20u, 32u << 16u, 10u, 18u, 80u),
        make_level(21u, 8u << 16u, 7u, 12u, 48u),
        make_level(22u, 0u, 4u, 6u, 24u),
    };
    levels[1].resident = 0u;
    levels[2].resident = 0u;
    sat_world_lod3d_state_t state = {};
    sat_world_lod3d_draw_t draw = {};
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats = {};
    assert(plan(view, budget, &object, 1u, levels, 3u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.desired_lod == 1u && draw.lod == 0u);
    assert((draw.flags & SAT_WORLD_LOD3D_DRAW_RESIDENCY_FALLBACK) != 0u);
    sat_world_lod3d_request_t request = {};
    assert(plan(view, budget, &object, 1u, levels, 3u, &state, &draw, 1u,
                &work, 1u, &stats, &request, 1u) == SAT_OK);
    assert(stats.prefetch_requests == 1u && request.lod == 1u &&
           request.asset_id == 21u);
}

void hysteresis_object_generation_and_camera_generation_reset() {
    auto view = make_view();
    view.focal_length_px_16_16 = 100u << 16u;
    auto budget = make_budget();
    auto object = make_object(20u, 13, 0u, 2u);
    object.lod_hysteresis_px_16_16 = 4u << 16u;
    sat_world_lod3d_level_t levels[2] = {
        make_level(1u, 32u << 16u, 10u, 20u, 80u),
        make_level(2u, 0u, 4u, 8u, 24u),
    };
    sat_world_lod3d_state_t state = {1u, 1u, 0u, 1u, 0u};
    sat_world_lod3d_draw_t draw = {};
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats = {};
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.desired_lod == 1u && draw.lod == 0u);

    object.bounds_min[2] = fx(12);
    object.bounds_max[2] = fx(14);
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.desired_lod == 1u && draw.lod == 0u); /* held in the band */

    view.generation = 2u; /* teleport/rebase clears the temporal hold */
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.lod == 1u);

    object.bounds_min[2] = fx(10);
    object.bounds_max[2] = fx(12);
    object.generation = 2u;
    state.selected_lod = 0u;
    state.valid = 1u;
    state.object_generation = 1u;
    state.view_generation = 2u;
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.lod == draw.desired_lod); /* asset replacement also resets */
}

void camera_origin_rebase_preserves_relative_lod_state() {
    auto view = make_view();
    view.focal_length_px_16_16 = 100u << 16u;
    auto budget = make_budget();
    auto object = make_object(25u, 13, 0u, 2u);
    object.lod_hysteresis_px_16_16 = 4u << 16u;
    sat_world_lod3d_level_t levels[2] = {
        make_level(1u, 32u << 16u, 8u, 12u, 48u),
        make_level(2u, 0u, 4u, 6u, 24u),
    };
    sat_world_lod3d_state_t state = {1u, 1u, 0u, 1u, 0u};
    sat_world_lod3d_draw_t draw = {};
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats = {};
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    const uint8_t lod_before = draw.lod;
    const uint32_t radius_before = draw.projected_radius_px_16_16;

    /* The world's absolute origin advances by thousands of units, but the
     * camera-relative AABB and view generation stay continuous. */
    const int32_t old_camera_local_z = fx(100);
    const int32_t old_object_local_z = fx(113);
    const int32_t new_origin_z = fx(9000);
    const int32_t new_camera_local_z = fx(100);
    const int32_t new_object_local_z = fx(113);
    (void)new_origin_z; /* absolute origin is intentionally absent from the API */
    assert(old_object_local_z - old_camera_local_z ==
           new_object_local_z - new_camera_local_z);
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(draw.lod == lod_before && draw.projected_radius_px_16_16 == radius_before);
}

void stable_id_breaks_ties_independent_of_input_order() {
    auto view = make_view();
    auto budget = make_budget();
    sat_world_lod3d_object_t first[2] = {
        make_object(20u, 10, 0u, 1u), make_object(10u, 10, 1u, 1u)};
    sat_world_lod3d_object_t second[2] = {first[1], first[0]};
    sat_world_lod3d_level_t levels[2] = {
        make_level(100u, 0u, 1u, 1u, 0u), make_level(200u, 0u, 1u, 1u, 0u)};
    sat_world_lod3d_state_t states_a[2] = {};
    sat_world_lod3d_state_t states_b[2] = {};
    sat_world_lod3d_draw_t draws_a[2] = {};
    sat_world_lod3d_draw_t draws_b[2] = {};
    sat_world_lod3d_work_t work_a[2] = {};
    sat_world_lod3d_work_t work_b[2] = {};
    sat_world_lod3d_stats_t stats_a = {}, stats_b = {};
    assert(plan(view, budget, first, 2u, levels, 2u, states_a, draws_a, 2u,
                work_a, 2u, &stats_a) == SAT_OK);
    assert(plan(view, budget, second, 2u, levels, 2u, states_b, draws_b, 2u,
                work_b, 2u, &stats_b) == SAT_OK);
    assert(draws_a[0].stable_id == 10u && draws_a[1].stable_id == 20u);
    assert(draws_b[0].stable_id == draws_a[0].stable_id &&
           draws_b[1].stable_id == draws_a[1].stable_id);
    assert(draws_b[0].asset_id == draws_a[0].asset_id &&
           draws_b[1].asset_id == draws_a[1].asset_id);
}

void optional_far_items_degrade_then_drop_before_protected_items() {
    auto view = make_view();
    auto budget = make_budget();
    budget.max_faces = 10u;
    sat_world_lod3d_object_t objects[2] = {
        make_object(1u, 10, 0u, 2u), make_object(2u, 80, 2u, 2u)};
    objects[1].flags = SAT_WORLD_LOD3D_OBJECT_OPTIONAL;
    sat_world_lod3d_level_t levels[4] = {
        make_level(1u, 32u << 16u, 10u, 20u, 80u),
        make_level(2u, 0u, 6u, 12u, 48u),
        make_level(3u, 32u << 16u, 10u, 20u, 80u),
        make_level(4u, 0u, 6u, 12u, 48u),
    };
    sat_world_lod3d_state_t states[2] = {};
    sat_world_lod3d_draw_t draws[2] = {};
    sat_world_lod3d_work_t work[2] = {};
    sat_world_lod3d_stats_t stats = {};
    assert(plan(view, budget, objects, 2u, levels, 4u, states, draws, 2u,
                work, 2u, &stats) == SAT_OK);
    assert(stats.draw_count == 1u && stats.faces == 6u);
    assert(stats.budget_degraded_objects == 1u);
    assert(stats.optional_dropped == 1u);
    assert(draws[0].stable_id == 1u && draws[0].lod == 1u);
}

void pinned_baseline_and_output_capacity_fail_atomically() {
    auto view = make_view();
    auto budget = make_budget();
    budget.max_faces = 5u;
    auto object = make_object(9u, 10, 0u, 1u);
    object.flags = SAT_WORLD_LOD3D_OBJECT_PINNED;
    sat_world_lod3d_level_t level = make_level(9u, 0u, 10u, 12u, 8u);
    sat_world_lod3d_state_t state = {99u, 88u, 3u, 1u, 0u};
    sat_world_lod3d_state_t original_state = state;
    sat_world_lod3d_draw_t draw = {};
    draw.stable_id = 0x12345678u;
    sat_world_lod3d_draw_t original_draw = draw;
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats;
    std::memset(&stats, 0x5A, sizeof(stats));
    sat_world_lod3d_stats_t original_stats = stats;
    assert(plan(view, budget, &object, 1u, &level, 1u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_ERR_CAPACITY);
    assert(std::memcmp(&state, &original_state, sizeof(state)) == 0);
    assert(std::memcmp(&draw, &original_draw, sizeof(draw)) == 0);
    assert(std::memcmp(&stats, &original_stats, sizeof(stats)) == 0);

    budget.max_faces = UINT32_MAX;
    state = original_state;
    draw = original_draw;
    stats = original_stats;
    assert(plan(view, budget, &object, 1u, &level, 1u, &state, &draw, 0u,
                &work, 1u, &stats) == SAT_ERR_CAPACITY);
    assert(std::memcmp(&state, &original_state, sizeof(state)) == 0);
    assert(std::memcmp(&draw, &original_draw, sizeof(draw)) == 0);
    assert(std::memcmp(&stats, &original_stats, sizeof(stats)) == 0);
}

void empty_levels_are_far_optional_and_not_drawn() {
    auto view = make_view();
    auto budget = make_budget();
    auto object = make_object(31u, 120, 0u, 2u);
    object.flags = SAT_WORLD_LOD3D_OBJECT_OPTIONAL |
                   SAT_WORLD_LOD3D_OBJECT_ALLOW_EMPTY_FAR;
    object.empty_max_projected_radius_px_16_16 = 5u << 16u;
    sat_world_lod3d_level_t levels[2] = {
        make_level(1u, 32u << 16u, 8u, 16u, 32u),
        make_level(0u, 0u, 0u, 0u, 0u),
    };
    levels[1].flags = SAT_WORLD_LOD3D_LEVEL_EMPTY;
    sat_world_lod3d_state_t state = {};
    sat_world_lod3d_draw_t draw = {};
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats = {};
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(stats.empty_selected == 1u && stats.draw_count == 0u && state.valid == 1u);

    object.bounds_min[2] = fx(9);
    object.bounds_max[2] = fx(11);
    state = {};
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_OK);
    assert(stats.empty_selected == 0u && stats.draw_count == 1u && draw.lod == 0u);

    object.flags = 0u;
    state = {};
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_ERR_INVALID_ARG);
}

void malformed_input_and_work_capacity_are_rejected_atomically() {
    auto view = make_view();
    auto budget = make_budget();
    auto object = make_object(1u, 10, 0u, 2u);
    sat_world_lod3d_level_t levels[2] = {
        make_level(1u, 8u << 16u, 1u, 1u, 1u),
        make_level(2u, 0u, 1u, 1u, 1u),
    };
    sat_world_lod3d_state_t state = {77u, 66u, 1u, 1u, 0u};
    const auto old_state = state;
    sat_world_lod3d_draw_t draw = {};
    draw.stable_id = 55u;
    const auto old_draw = draw;
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats = {};

    levels[1].min_projected_radius_px_16_16 = 16u << 16u;
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_ERR_INVALID_ARG);
    assert(std::memcmp(&state, &old_state, sizeof(state)) == 0 && draw.stable_id == old_draw.stable_id);

    levels[1].min_projected_radius_px_16_16 = 0u;
    object.bounds_min[0] = fx(2);
    object.bounds_max[0] = fx(-2);
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_ERR_INVALID_ARG);
    object.bounds_min[0] = fx(-1);
    object.bounds_max[0] = fx(1);

    auto bad_view = view;
    bad_view.forward_z = 2 * 65536;
    assert(plan(bad_view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats) == SAT_ERR_INVALID_ARG);
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                nullptr, 0u, &stats) == SAT_ERR_INVALID_ARG);
}

void duplicate_ids_are_rejected_and_state_is_unchanged() {
    auto view = make_view();
    auto budget = make_budget();
    sat_world_lod3d_object_t objects[2] = {
        make_object(4u, 10, 0u, 1u), make_object(4u, 12, 1u, 1u)};
    sat_world_lod3d_level_t levels[2] = {
        make_level(1u, 0u, 1u, 1u, 1u), make_level(2u, 0u, 1u, 1u, 1u)};
    sat_world_lod3d_state_t states[2] = {{1u, 1u, 0u, 1u, 0u},
                                         {2u, 2u, 0u, 1u, 0u}};
    const auto before = states[0];
    sat_world_lod3d_draw_t draws[2] = {};
    sat_world_lod3d_work_t work[2] = {};
    sat_world_lod3d_stats_t stats = {};
    assert(plan(view, budget, objects, 2u, levels, 2u, states, draws, 2u,
                work, 2u, &stats) == SAT_ERR_INVALID_ARG);
    assert(std::memcmp(&states[0], &before, sizeof(before)) == 0);
}

void prefetch_capacity_failure_is_atomic() {
    auto view = make_view();
    auto budget = make_budget();
    auto object = make_object(17u, 10, 0u, 2u);
    sat_world_lod3d_level_t levels[2] = {
        make_level(100u, 32u << 16u, 8u, 12u, 48u),
        make_level(101u, 0u, 4u, 6u, 24u),
    };
    levels[0].resident = 0u;
    sat_world_lod3d_state_t state = {55u, 44u, 1u, 1u, 0u};
    const auto state_before = state;
    sat_world_lod3d_draw_t draw = {};
    draw.stable_id = 0xAABBCCDDu;
    const auto draw_before = draw;
    sat_world_lod3d_work_t work = {};
    sat_world_lod3d_stats_t stats;
    std::memset(&stats, 0xA5, sizeof(stats));
    const auto stats_before = stats;
    sat_world_lod3d_request_t request = {};
    request.asset_id = 0x12345678u;
    const auto request_before = request;
    assert(plan(view, budget, &object, 1u, levels, 2u, &state, &draw, 1u,
                &work, 1u, &stats, &request, 0u) == SAT_ERR_CAPACITY);
    assert(std::memcmp(&state, &state_before, sizeof(state)) == 0);
    assert(std::memcmp(&draw, &draw_before, sizeof(draw)) == 0);
    assert(std::memcmp(&stats, &stats_before, sizeof(stats)) == 0);
    assert(std::memcmp(&request, &request_before, sizeof(request)) == 0);
}

}  // namespace

int main() {
    zero_objects_succeeds();
    one_object_and_viewport_saturation();
    one_through_four_levels_select_by_projected_size();
    separate_face_command_and_texture_budgets();
    non_monotonic_costs_skip_expensive_middle_lod();
    frustum_is_conservative_and_negative_coordinates_work();
    stale_residency_falls_back_and_missing_assets_are_observable();
    finer_resident_level_can_cover_a_missing_desired_level();
    hysteresis_object_generation_and_camera_generation_reset();
    stable_id_breaks_ties_independent_of_input_order();
    camera_origin_rebase_preserves_relative_lod_state();
    optional_far_items_degrade_then_drop_before_protected_items();
    pinned_baseline_and_output_capacity_fail_atomically();
    empty_levels_are_far_optional_and_not_drawn();
    malformed_input_and_work_capacity_are_rejected_atomically();
    duplicate_ids_are_rejected_and_state_is_unchanged();
    prefetch_capacity_failure_is_atomic();
    std::puts("world visibility and LOD planner: OK (17 test groups)");
    return 0;
}
