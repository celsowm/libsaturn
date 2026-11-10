#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "saturn/world_lod3d_governor.h"

namespace {

sat_world_lod3d_governor_config_t make_config() {
    sat_world_lod3d_governor_config_t config = {};
    assert(sat_world_lod3d_governor_config_default(&config) == SAT_OK);
    config.enabled = 1u;
    config.target_fields = 2u;
    config.cooldown_fields = 0u;
    config.overload_samples = 2u;
    config.recovery_samples = 4u;
    config.degrade_percent = 20u;
    config.recover_percent = 5u;
    config.recovery_headroom_percent = 75u;
    config.minimum_faces = 100u;
    config.minimum_commands = 100u;
    config.minimum_texture_bytes = 100u;
    config.target_cpu_frt_ticks = 1000u;
    return config;
}

sat_world_lod3d_budget_t make_budget() {
    sat_world_lod3d_budget_t budget = {};
    budget.max_faces = 1000u;
    budget.max_commands = 900u;
    budget.max_texture_bytes = 800u;
    budget.max_budget_lod_loss = 3u;
    return budget;
}

sat_world_lod3d_governor_telemetry_t make_telemetry(uint32_t field) {
    sat_world_lod3d_governor_telemetry_t telemetry = {};
    telemetry.scene.commands_capacity = 1000u;
    telemetry.scene.overlay_reserved = 100u;
    telemetry.scene.commands_used = 100u;
    telemetry.scene.world_commands = 100u;
    telemetry.scene.result = SAT_OK;
    telemetry.field_count = field;
    telemetry.cpu_frt_ticks = 500u;
    telemetry.profile_available = 1u;
    return telemetry;
}

sat_result_t step(const sat_world_lod3d_governor_config_t& config,
                  const sat_world_lod3d_governor_telemetry_t& telemetry,
                  const sat_world_lod3d_budget_t& base,
                  sat_world_lod3d_governor_state_t* state,
                  sat_world_lod3d_budget_t* budget,
                  sat_world_lod3d_governor_stats_t* stats) {
    return sat_world_lod3d_governor_step(
        &config, &telemetry, &base, state, budget, stats);
}

void governor_is_opt_in_and_clamps_commands_around_hud_reserve() {
    sat_world_lod3d_governor_config_t disabled = {};
    assert(sat_world_lod3d_governor_config_default(&disabled) == SAT_OK);
    assert(disabled.enabled == 0u);
    auto base = make_budget();
    auto telemetry = make_telemetry(100u);
    sat_world_lod3d_governor_state_t state = {};
    sat_world_lod3d_budget_t budget = {};
    sat_world_lod3d_governor_stats_t stats = {};
    assert(step(disabled, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_faces == base.max_faces);
    assert(budget.max_commands == 900u); /* 1000 total - 100 HUD reserve */
    assert(budget.max_texture_bytes == base.max_texture_bytes);
    assert(stats.transition == SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_DISABLED);
    assert(state.overload_streak == 0u && state.recovery_transitions == 0u);
}

void stable_load_preserves_the_user_selected_ceiling() {
    auto config = make_config();
    auto base = make_budget();
    auto telemetry = make_telemetry(10u);
    sat_world_lod3d_governor_state_t state = {};
    sat_world_lod3d_budget_t budget = {};
    sat_world_lod3d_governor_stats_t stats = {};
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    for (uint32_t i = 0u; i < 20u; ++i) {
        telemetry.field_count += 2u;
        telemetry.cpu_frt_ticks = 700u;
        assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
        assert(budget.max_faces == base.max_faces);
        assert(budget.max_commands == 900u);
        assert(budget.max_texture_bytes == base.max_texture_bytes);
        assert(stats.transition == SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_NONE);
    }
}

void persistent_overload_degrades_only_after_confirmation_and_cooldown() {
    auto config = make_config();
    config.cooldown_fields = 4u;
    auto base = make_budget();
    auto telemetry = make_telemetry(100u);
    sat_world_lod3d_governor_state_t state = {};
    sat_world_lod3d_budget_t budget = {};
    sat_world_lod3d_governor_stats_t stats = {};
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(stats.transition == SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_INITIALIZED);

    telemetry.field_count = 102u;
    telemetry.scene.budget_blocked_faces = 1u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_faces == 1000u && state.overload_streak == 1u);
    telemetry.field_count = 104u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(stats.transition == SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_DEGRADED);
    assert(stats.overload_reason & SAT_WORLD_LOD3D_GOVERNOR_REASON_FACE_BUDGET);
    assert(budget.max_faces == 800u && budget.max_commands == 720u);
    assert(budget.max_texture_bytes == 640u && stats.degraded_transitions == 1u);

    telemetry.field_count = 106u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_faces == 800u); /* cooldown is still active */
    telemetry.field_count = 108u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_faces == 640u && stats.degraded_transitions == 2u);
}

void transient_spikes_pause_and_profile_disabled_do_not_fake_overload() {
    auto config = make_config();
    auto base = make_budget();
    auto telemetry = make_telemetry(10u);
    sat_world_lod3d_governor_state_t state = {};
    sat_world_lod3d_budget_t budget = {};
    sat_world_lod3d_governor_stats_t stats = {};
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);

    telemetry.field_count = 12u;
    telemetry.scene.result = SAT_ERR_CAPACITY;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_faces == base.max_faces && state.overload_streak == 1u);

    telemetry.field_count = 13u;
    telemetry.scene.result = SAT_OK;
    telemetry.scene.budget_blocked_faces = 0u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(state.overload_streak == 0u && state.headroom_streak == 1u);
    telemetry.field_count = 20u;
    telemetry.paused = 1u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(state.overload_streak == 0u && state.headroom_streak == 0u);
    telemetry.field_count = 100u;
    telemetry.paused = 0u;
    telemetry.profile_available = 0u;
    telemetry.cpu_frt_ticks = UINT32_MAX;
    telemetry.scene.result = SAT_OK;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(stats.elapsed_fields == 80u);
    assert(stats.profile_available == 0u && stats.cpu_frt_ticks == 0u);
    assert((stats.overload_reason & SAT_WORLD_LOD3D_GOVERNOR_REASON_CPU_TIME) == 0u);
    assert((stats.overload_reason & SAT_WORLD_LOD3D_GOVERNOR_REASON_FIELD_TIME) != 0u);
}

void recovery_is_slower_and_uses_only_sustained_headroom() {
    auto config = make_config();
    auto base = make_budget();
    auto telemetry = make_telemetry(20u);
    sat_world_lod3d_governor_state_t state = {};
    sat_world_lod3d_budget_t budget = {};
    sat_world_lod3d_governor_stats_t stats = {};
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);

    telemetry.scene.budget_blocked_faces = 1u;
    telemetry.field_count = 22u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    telemetry.field_count = 24u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_faces == 800u);
    telemetry.scene.budget_blocked_faces = 0u;
    telemetry.cpu_frt_ticks = 100u;
    for (uint32_t i = 0u; i < 3u; ++i) {
        telemetry.field_count += 1u;
        assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
        assert(budget.max_faces == 800u);
    }
    telemetry.field_count += 1u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(stats.transition == SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_RECOVERED);
    assert(budget.max_faces == 810u && stats.recovery_transitions == 1u);
    assert(budget.max_faces - 800u < 1000u - 800u);
}

void wrapped_field_counter_and_real_command_capacity_are_observed() {
    auto config = make_config();
    config.overload_samples = 1u;
    config.recovery_samples = 2u;
    config.minimum_commands = 100u;
    auto base = make_budget();
    auto telemetry = make_telemetry(UINT32_MAX - 1u);
    telemetry.scene.commands_capacity = 500u;
    telemetry.scene.overlay_reserved = 60u;
    telemetry.scene.commands_used = 439u;
    telemetry.scene.world_commands = 439u;
    sat_world_lod3d_governor_state_t state = {};
    sat_world_lod3d_budget_t budget = {};
    sat_world_lod3d_governor_stats_t stats = {};
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_commands == 440u && stats.usable_world_commands == 440u);

    telemetry.field_count = 1u;
    telemetry.scene.commands_used = 440u;
    telemetry.scene.world_commands = 440u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(stats.elapsed_fields == 3u);
    assert((stats.overload_reason & SAT_WORLD_LOD3D_GOVERNOR_REASON_COMMAND_CAPACITY) != 0u);
    assert(budget.max_commands < 440u);
}

void command_budget_preserves_commands_emitted_before_the_scene() {
    auto config = make_config();
    auto base = make_budget();
    auto telemetry = make_telemetry(10u);
    telemetry.scene.commands_capacity = 500u;
    telemetry.scene.overlay_reserved = 60u;
    telemetry.scene.commands_used = 150u;
    telemetry.scene.world_commands = 50u;
    sat_world_lod3d_governor_state_t state = {};
    sat_world_lod3d_budget_t budget = {};
    sat_world_lod3d_governor_stats_t stats = {};
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(stats.usable_world_commands == 340u);
    assert(budget.max_commands == 340u);

    /* A malformed scene snapshot must fail atomically rather than wrap. */
    telemetry.field_count = 12u;
    telemetry.scene.world_commands = 151u;
    const auto state_before = state;
    const auto budget_before = budget;
    const auto stats_before = stats;
    assert(step(config, telemetry, base, &state, &budget, &stats) ==
           SAT_ERR_INVALID_ARG);
    assert(std::memcmp(&state, &state_before, sizeof(state)) == 0);
    assert(std::memcmp(&budget, &budget_before, sizeof(budget)) == 0);
    assert(std::memcmp(&stats, &stats_before, sizeof(stats)) == 0);
}

void impossible_floors_and_invalid_samples_fail_atomically() {
    auto config = make_config();
    auto base = make_budget();
    auto telemetry = make_telemetry(1u);
    telemetry.scene.commands_capacity = 300u;
    telemetry.scene.overlay_reserved = 250u;
    sat_world_lod3d_governor_state_t state = {};
    state.selected_faces = 550u;
    state.selected_commands = 450u;
    state.selected_texture_bytes = 350u;
    state.last_field_count = 77u;
    state.overload_streak = 1u;
    state.valid = 1u;
    sat_world_lod3d_budget_t budget = {};
    sat_world_lod3d_governor_stats_t stats = {};
    std::memset(&budget, 0xA5, sizeof(budget));
    std::memset(&stats, 0xA5, sizeof(stats));
    const auto state_before = state;
    const auto budget_before = budget;
    const auto stats_before = stats;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_ERR_CAPACITY);
    assert(std::memcmp(&state, &state_before, sizeof(state)) == 0);
    assert(std::memcmp(&budget, &budget_before, sizeof(budget)) == 0);
    assert(std::memcmp(&stats, &stats_before, sizeof(stats)) == 0);

    telemetry.scene.overlay_reserved = 250u;
    telemetry.scene.result = SAT_ERR_BUSY;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_ERR_BUSY);
    assert(std::memcmp(&state, &state_before, sizeof(state)) == 0);

    telemetry.scene.overlay_reserved = 301u;
    telemetry.scene.result = SAT_OK;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_ERR_INVALID_ARG);
    assert(std::memcmp(&state, &state_before, sizeof(state)) == 0);
}

void budget_floors_saturate_and_cpu_time_is_an_independent_signal() {
    auto config = make_config();
    config.overload_samples = 1u;
    config.recovery_samples = 2u;
    config.minimum_faces = 990u;
    config.minimum_commands = 890u;
    config.minimum_texture_bytes = 790u;
    auto base = make_budget();
    auto telemetry = make_telemetry(10u);
    sat_world_lod3d_governor_state_t state = {};
    sat_world_lod3d_budget_t budget = {};
    sat_world_lod3d_governor_stats_t stats = {};
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);

    telemetry.field_count = 12u;
    telemetry.scene.budget_blocked_faces = 1u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_faces == 990u && budget.max_commands == 890u);
    assert(budget.max_texture_bytes == 790u && stats.at_minimum == 1u);

    telemetry.field_count = 14u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_faces == 990u && stats.degraded_transitions == 1u);

    /* Reset and use CPU time alone as persistent measured overload. */
    config.overload_samples = 2u;
    config.recovery_samples = 3u;
    state = {};
    telemetry = make_telemetry(100u);
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    telemetry.scene.budget_blocked_faces = 0u;
    telemetry.field_count = 102u;
    telemetry.cpu_frt_ticks = 1500u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(budget.max_faces == base.max_faces);
    telemetry.field_count = 104u;
    assert(step(config, telemetry, base, &state, &budget, &stats) == SAT_OK);
    assert(stats.overload_reason & SAT_WORLD_LOD3D_GOVERNOR_REASON_CPU_TIME);
    assert(stats.degraded_transitions == 1u && budget.max_faces < base.max_faces);
}

}  // namespace

int main() {
    governor_is_opt_in_and_clamps_commands_around_hud_reserve();
    stable_load_preserves_the_user_selected_ceiling();
    persistent_overload_degrades_only_after_confirmation_and_cooldown();
    transient_spikes_pause_and_profile_disabled_do_not_fake_overload();
    recovery_is_slower_and_uses_only_sustained_headroom();
    wrapped_field_counter_and_real_command_capacity_are_observed();
    command_budget_preserves_commands_emitted_before_the_scene();
    impossible_floors_and_invalid_samples_fail_atomically();
    budget_floors_saturate_and_cpu_time_is_an_independent_signal();
    std::puts("world LOD budget governor: OK (9 test groups)");
    return 0;
}
