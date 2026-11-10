#ifndef SATURN_WORLD_LOD3D_GOVERNOR_H
#define SATURN_WORLD_LOD3D_GOVERNOR_H

#include <stdint.h>

#include "saturn/scene.h"
#include "saturn/world_lod3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_WORLD_LOD3D_GOVERNOR_API_VERSION 1u

enum {
    SAT_WORLD_LOD3D_GOVERNOR_REASON_FACE_BUDGET = 1u << 0,
    SAT_WORLD_LOD3D_GOVERNOR_REASON_COMMAND_BUDGET = 1u << 1,
    SAT_WORLD_LOD3D_GOVERNOR_REASON_COMMAND_CAPACITY = 1u << 2,
    SAT_WORLD_LOD3D_GOVERNOR_REASON_SCENE_CAPACITY = 1u << 3,
    SAT_WORLD_LOD3D_GOVERNOR_REASON_FIELD_TIME = 1u << 4,
    SAT_WORLD_LOD3D_GOVERNOR_REASON_CPU_TIME = 1u << 5
};

typedef enum sat_world_lod3d_governor_transition {
    SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_NONE = 0,
    SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_INITIALIZED = 1,
    SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_DEGRADED = 2,
    SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_RECOVERED = 3,
    SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_DISABLED = 4,
    SAT_WORLD_LOD3D_GOVERNOR_TRANSITION_PAUSED = 5
} sat_world_lod3d_governor_transition_t;

/* The base budget passed to step() is the caller-selected ceiling. The
 * governor can move down to the minimums, then recover slowly to that ceiling.
 * A zero target_cpu_frt_ticks disables CPU-time decisions; field cadence and
 * actual scene-capacity feedback remain usable when profiling is absent. */
typedef struct sat_world_lod3d_governor_config {
    uint32_t api_version;
    uint32_t struct_size;
    uint32_t target_cpu_frt_ticks;
    uint32_t minimum_faces;
    uint32_t minimum_commands;
    uint32_t minimum_texture_bytes;
    uint16_t target_fields; /* e.g. 2 fields for a 30 Hz target */
    uint16_t cooldown_fields;
    uint8_t enabled; /* default off */
    uint8_t overload_samples;
    uint8_t recovery_samples;
    uint8_t degrade_percent;
    uint8_t recover_percent;
    uint8_t recovery_headroom_percent;
    uint8_t reserved[2];
} sat_world_lod3d_governor_config_t;

/* Initialize with conservative, opt-in defaults. Callers still set their
 * scene-specific ceilings and minimums before enabling. */
sat_result_t sat_world_lod3d_governor_config_default(
    sat_world_lod3d_governor_config_t* out);

typedef struct sat_world_lod3d_governor_telemetry {
    sat_scene_stats_t scene; /* captured after sat_scene_flush() */
    uint32_t field_count;    /* sat_field_count(), wrap-safe by subtraction */
    uint32_t cpu_frt_ticks;  /* measured frame preparation/execution interval */
    uint8_t profile_available; /* 0 means CPU ticks are ignored, not zero-cost */
    uint8_t paused;            /* suppress decisions and reset persistence windows */
    uint16_t reserved;
} sat_world_lod3d_governor_telemetry_t;

typedef struct sat_world_lod3d_governor_state {
    uint32_t selected_faces;
    uint32_t selected_commands;
    uint32_t selected_texture_bytes;
    uint32_t last_field_count;
    uint32_t last_transition_field;
    uint64_t fields_ewma_q8;
    uint64_t cpu_ewma_q8;
    uint32_t samples;
    uint32_t degraded_transitions;
    uint32_t recovery_transitions;
    uint16_t overload_streak;
    uint16_t headroom_streak;
    uint8_t valid;
    uint8_t transition_valid;
    uint16_t reserved;
} sat_world_lod3d_governor_state_t;

typedef struct sat_world_lod3d_governor_stats {
    uint32_t elapsed_fields;
    uint32_t cpu_frt_ticks;
    uint32_t sample_count;
    uint32_t degraded_transitions;
    uint32_t recovery_transitions;
    uint64_t fields_ewma_q8;
    uint64_t cpu_ewma_q8;
    sat_result_t scene_result;
    uint16_t world_commands;
    uint16_t usable_world_commands;
    uint16_t budget_blocked_faces;
    uint16_t clipped_faces;
    uint16_t fallback_faces;
    uint16_t rejected_faces;
    uint8_t profile_available;
    uint8_t overload_reason;
    uint8_t transition;
    uint8_t at_minimum;
} sat_world_lod3d_governor_stats_t;

/* Update once after a completed scene and measured field interval. The
 * governor only changes the planner's face/command/texture ceilings. It never
 * edits VDP1 reservations, HUD commands, collision quality or pinned objects.
 * Command budget is clamped to commands_capacity minus commands already used
 * before the scene and the overlay reservation when the scene reports a
 * capacity. `commands_used` is the post-flush snapshot, and
 * `world_commands` is the scene's contribution. State, telemetry, base budget
 * and the two outputs must not overlap. `state`, `out_budget` and `out_stats` are unchanged on
 * invalid input or impossible configured minimums. */
sat_result_t sat_world_lod3d_governor_step(
    const sat_world_lod3d_governor_config_t* config,
    const sat_world_lod3d_governor_telemetry_t* telemetry,
    const sat_world_lod3d_budget_t* base_budget,
    sat_world_lod3d_governor_state_t* state,
    sat_world_lod3d_budget_t* out_budget,
    sat_world_lod3d_governor_stats_t* out_stats);

#ifdef __cplusplus
}
#endif

#endif
