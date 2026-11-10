#include "saturn/world_lod3d_governor.h"

#include <limits.h>

namespace {

uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }
uint32_t max_u32(uint32_t a, uint32_t b) { return a > b ? a : b; }

uint32_t saturating_increment(uint32_t value) {
    return value == UINT32_MAX ? value : value + 1u;
}

uint16_t saturating_increment(uint16_t value) {
    return value == UINT16_MAX ? value : static_cast<uint16_t>(value + 1u);
}

uint64_t ewma_q8(uint64_t previous, uint32_t sample) {
    const uint64_t sample_q8 = static_cast<uint64_t>(sample) << 8u;
    if (previous == 0u) return sample_q8;
    if (sample_q8 >= previous) return previous + (sample_q8 - previous) / 4u;
    return previous - (previous - sample_q8) / 4u;
}

bool valid_config(const sat_world_lod3d_governor_config_t& config) {
    return config.api_version == SAT_WORLD_LOD3D_GOVERNOR_API_VERSION &&
           config.struct_size == sizeof(sat_world_lod3d_governor_config_t) &&
           config.enabled <= 1u && config.target_fields != 0u &&
           config.overload_samples != 0u &&
           config.recovery_samples > config.overload_samples &&
           config.degrade_percent != 0u && config.degrade_percent <= 50u &&
           config.recover_percent != 0u &&
           config.recover_percent <= config.degrade_percent &&
           config.recovery_headroom_percent != 0u &&
           config.recovery_headroom_percent < 100u;
}

bool valid_state(const sat_world_lod3d_governor_state_t& state) {
    return state.valid <= 1u && state.transition_valid <= 1u;
}

uint32_t step_down(uint32_t value, uint32_t floor, uint8_t percent) {
    if (value <= floor) return value;
    uint64_t amount =
        (static_cast<uint64_t>(value) * percent + 99u) / 100u;
    if (amount == 0u) amount = 1u;
    if (amount >= static_cast<uint64_t>(value) - floor) return floor;
    return value - static_cast<uint32_t>(amount);
}

uint32_t step_up(uint32_t value, uint32_t ceiling, uint8_t percent) {
    if (value >= ceiling) return ceiling;
    const uint32_t gap = ceiling - value;
    uint64_t amount =
        (static_cast<uint64_t>(gap) * percent + 99u) / 100u;
    if (amount == 0u) amount = 1u;
    if (amount >= gap) return ceiling;
    return value + static_cast<uint32_t>(amount);
}

bool at_minimum(const sat_world_lod3d_governor_state_t& state,
                const sat_world_lod3d_governor_config_t& config) {
    return state.selected_faces <= config.minimum_faces &&
           state.selected_commands <= config.minimum_commands &&
           state.selected_texture_bytes <= config.minimum_texture_bytes;
}

void fill_stats(const sat_world_lod3d_governor_state_t& state,
                const sat_world_lod3d_governor_telemetry_t& telemetry,
                uint32_t elapsed_fields, uint16_t usable_commands,
                uint8_t profile_available, uint8_t reason, uint8_t transition,
                const sat_world_lod3d_governor_config_t& config,
                sat_world_lod3d_governor_stats_t* stats) {
    *stats = {};
    stats->elapsed_fields = elapsed_fields;
    stats->cpu_frt_ticks = profile_available != 0u
        ? telemetry.cpu_frt_ticks : 0u;
    stats->sample_count = state.samples;
    stats->degraded_transitions = state.degraded_transitions;
    stats->recovery_transitions = state.recovery_transitions;
    stats->fields_ewma_q8 = state.fields_ewma_q8;
    stats->cpu_ewma_q8 = state.cpu_ewma_q8;
    stats->scene_result = telemetry.scene.result;
    stats->world_commands = telemetry.scene.world_commands;
    stats->usable_world_commands = usable_commands;
    stats->budget_blocked_faces = telemetry.scene.budget_blocked_faces;
    stats->clipped_faces = telemetry.scene.clipped_faces;
    stats->fallback_faces = telemetry.scene.fallback_faces;
    stats->rejected_faces = telemetry.scene.rejected_faces;
    stats->profile_available = profile_available;
    stats->overload_reason = reason;
    stats->transition = transition;
    stats->at_minimum = at_minimum(state, config) ? 1u : 0u;
}

}  // namespace

extern "C" sat_result_t sat_world_lod3d_governor_config_default(
    sat_world_lod3d_governor_config_t* out) {
    if (out == nullptr) return SAT_ERR_INVALID_ARG;
    sat_world_lod3d_governor_config_t config = {};
    config.api_version = SAT_WORLD_LOD3D_GOVERNOR_API_VERSION;
    config.struct_size = sizeof(config);
    config.target_fields = 2u;
    config.cooldown_fields = 4u;
    config.enabled = 0u;
    config.overload_samples = 3u;
    config.recovery_samples = 12u;
    config.degrade_percent = 12u;
    config.recover_percent = 4u;
    config.recovery_headroom_percent = 75u;
    *out = config;
    return SAT_OK;
}

extern "C" sat_result_t sat_world_lod3d_governor_step(
    const sat_world_lod3d_governor_config_t* config,
    const sat_world_lod3d_governor_telemetry_t* telemetry,
    const sat_world_lod3d_budget_t* base_budget,
    sat_world_lod3d_governor_state_t* state,
    sat_world_lod3d_budget_t* out_budget,
    sat_world_lod3d_governor_stats_t* out_stats) {
    if (config == nullptr || telemetry == nullptr || base_budget == nullptr ||
        state == nullptr || out_budget == nullptr || out_stats == nullptr ||
        !valid_config(*config) || !valid_state(*state) ||
        telemetry->profile_available > 1u || telemetry->paused > 1u ||
        telemetry->scene.overlay_reserved > telemetry->scene.commands_capacity ||
        (telemetry->scene.commands_capacity != 0u &&
         (telemetry->scene.commands_used > telemetry->scene.commands_capacity ||
          telemetry->scene.commands_used < telemetry->scene.world_commands))) {
        return SAT_ERR_INVALID_ARG;
    }
    if (telemetry->scene.result == SAT_ERR_BUSY) return SAT_ERR_BUSY;

    sat_world_lod3d_governor_state_t next_state = *state;
    sat_world_lod3d_governor_stats_t stats = {};
    sat_world_lod3d_budget_t budget = *base_budget;
    uint16_t usable_commands = 0u;
    uint32_t command_ceiling = base_budget->max_commands;
    if (telemetry->scene.commands_capacity != 0u) {
        const uint32_t preexisting_commands =
            telemetry->scene.commands_used - telemetry->scene.world_commands;
        const uint32_t non_world_commands =
            preexisting_commands + telemetry->scene.overlay_reserved;
        if (non_world_commands > telemetry->scene.commands_capacity) {
            return SAT_ERR_INVALID_ARG;
        }
        usable_commands = static_cast<uint16_t>(
            telemetry->scene.commands_capacity - non_world_commands);
        command_ceiling = min_u32(command_ceiling, usable_commands);
    }
    if (config->minimum_faces > base_budget->max_faces ||
        config->minimum_commands > base_budget->max_commands ||
        config->minimum_texture_bytes > base_budget->max_texture_bytes) {
        return SAT_ERR_INVALID_ARG;
    }
    if (command_ceiling < config->minimum_commands) return SAT_ERR_CAPACITY;

    const uint8_t profile_sample =
        telemetry->profile_available != 0u && telemetry->cpu_frt_ticks != 0u;
    const uint8_t initial = next_state.valid == 0u;
    if (initial != 0u) {
        next_state = {};
        next_state.selected_faces = base_budget->max_faces;
        next_state.selected_commands = command_ceiling;
        next_state.selected_texture_bytes = base_budget->max_texture_bytes;
        next_state.last_field_count = telemetry->field_count;
        next_state.valid = 1u;
    } else {
        next_state.selected_faces = max_u32(config->minimum_faces,
            min_u32(next_state.selected_faces, base_budget->max_faces));
        next_state.selected_commands = max_u32(config->minimum_commands,
            min_u32(min_u32(next_state.selected_commands,
                            base_budget->max_commands), command_ceiling));
        next_state.selected_texture_bytes = max_u32(
            config->minimum_texture_bytes,
            min_u32(next_state.selected_texture_bytes,
                    base_budget->max_texture_bytes));
    }

    uint32_t elapsed_fields = 0u;
    uint8_t reason = 0u;
    uint8_t transition = SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_NONE;
    if (config->enabled == 0u) {
        next_state = {};
        next_state.selected_faces = base_budget->max_faces;
        next_state.selected_commands = command_ceiling;
        next_state.selected_texture_bytes = base_budget->max_texture_bytes;
        next_state.last_field_count = telemetry->field_count;
        next_state.valid = 1u;
        transition = SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_DISABLED;
    } else if (initial != 0u) {
        transition = SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_INITIALIZED;
    } else {
        elapsed_fields = telemetry->field_count - next_state.last_field_count;
        next_state.last_field_count = telemetry->field_count;
        if (telemetry->paused != 0u) {
            next_state.overload_streak = 0u;
            next_state.headroom_streak = 0u;
            transition = SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_PAUSED;
        } else if (elapsed_fields != 0u) {
            next_state.samples = saturating_increment(next_state.samples);
            next_state.fields_ewma_q8 = ewma_q8(
                next_state.fields_ewma_q8, elapsed_fields);
            if (profile_sample != 0u) {
                next_state.cpu_ewma_q8 = ewma_q8(
                    next_state.cpu_ewma_q8, telemetry->cpu_frt_ticks);
            }

            if (telemetry->scene.budget_blocked_faces != 0u)
                reason |= SAT_WORLD_LOD3D_GOVERNOR_REASON_FACE_BUDGET;
            if (telemetry->scene.world_commands > next_state.selected_commands)
                reason |= SAT_WORLD_LOD3D_GOVERNOR_REASON_COMMAND_BUDGET;
            if (telemetry->scene.commands_capacity != 0u &&
                telemetry->scene.world_commands >= usable_commands)
                reason |= SAT_WORLD_LOD3D_GOVERNOR_REASON_COMMAND_CAPACITY;
            if (telemetry->scene.result == SAT_ERR_CAPACITY)
                reason |= SAT_WORLD_LOD3D_GOVERNOR_REASON_SCENE_CAPACITY;
            if (elapsed_fields > config->target_fields)
                reason |= SAT_WORLD_LOD3D_GOVERNOR_REASON_FIELD_TIME;
            if (profile_sample != 0u && config->target_cpu_frt_ticks != 0u &&
                telemetry->cpu_frt_ticks > config->target_cpu_frt_ticks)
                reason |= SAT_WORLD_LOD3D_GOVERNOR_REASON_CPU_TIME;

            if (reason != 0u) {
                next_state.overload_streak = saturating_increment(
                    next_state.overload_streak);
                next_state.headroom_streak = 0u;
            } else {
                const uint8_t cpu_headroom = profile_sample == 0u ||
                    config->target_cpu_frt_ticks == 0u ||
                    static_cast<uint64_t>(telemetry->cpu_frt_ticks) * 100u <=
                        static_cast<uint64_t>(config->target_cpu_frt_ticks) *
                        config->recovery_headroom_percent;
                if (elapsed_fields < config->target_fields && cpu_headroom) {
                    next_state.headroom_streak = saturating_increment(
                        next_state.headroom_streak);
                    next_state.overload_streak = 0u;
                } else {
                    next_state.headroom_streak = 0u;
                    next_state.overload_streak = 0u;
                }
            }

            const uint8_t cooldown_done = next_state.transition_valid == 0u ||
                config->cooldown_fields == 0u ||
                static_cast<uint32_t>(telemetry->field_count -
                                      next_state.last_transition_field) >=
                    config->cooldown_fields;
            if (cooldown_done != 0u &&
                next_state.overload_streak >= config->overload_samples) {
                const uint32_t faces = step_down(
                    next_state.selected_faces, config->minimum_faces,
                    config->degrade_percent);
                const uint32_t commands = step_down(
                    next_state.selected_commands, config->minimum_commands,
                    config->degrade_percent);
                const uint32_t textures = step_down(
                    next_state.selected_texture_bytes,
                    config->minimum_texture_bytes, config->degrade_percent);
                if (faces != next_state.selected_faces ||
                    commands != next_state.selected_commands ||
                    textures != next_state.selected_texture_bytes) {
                    next_state.selected_faces = faces;
                    next_state.selected_commands = commands;
                    next_state.selected_texture_bytes = textures;
                    next_state.degraded_transitions = saturating_increment(
                        next_state.degraded_transitions);
                    next_state.last_transition_field = telemetry->field_count;
                    next_state.transition_valid = 1u;
                    transition = SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_DEGRADED;
                }
                next_state.overload_streak = 0u;
            } else if (cooldown_done != 0u &&
                       next_state.headroom_streak >= config->recovery_samples) {
                const uint32_t faces = step_up(
                    next_state.selected_faces, base_budget->max_faces,
                    config->recover_percent);
                const uint32_t commands = step_up(
                    next_state.selected_commands, command_ceiling,
                    config->recover_percent);
                const uint32_t textures = step_up(
                    next_state.selected_texture_bytes,
                    base_budget->max_texture_bytes, config->recover_percent);
                if (faces != next_state.selected_faces ||
                    commands != next_state.selected_commands ||
                    textures != next_state.selected_texture_bytes) {
                    next_state.selected_faces = faces;
                    next_state.selected_commands = commands;
                    next_state.selected_texture_bytes = textures;
                    next_state.recovery_transitions = saturating_increment(
                        next_state.recovery_transitions);
                    next_state.last_transition_field = telemetry->field_count;
                    next_state.transition_valid = 1u;
                    transition = SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_RECOVERED;
                }
                next_state.headroom_streak = 0u;
            }
        }
    }

    budget.max_faces = next_state.selected_faces;
    budget.max_commands = next_state.selected_commands;
    budget.max_texture_bytes = next_state.selected_texture_bytes;
    next_state.reserved = 0u;
    fill_stats(next_state, *telemetry, elapsed_fields, usable_commands,
               profile_sample, reason, transition, *config, &stats);
    *state = next_state;
    *out_budget = budget;
    *out_stats = stats;
    return SAT_OK;
}
