/* City ring adapter for the reusable world visibility/LOD planner. This file
 * translates fixed ring slots into immutable planner objects and translates
 * the selected IDs back into the renderer's compact draw descriptors. */
#include <stdint.h>

#include "city_walk.h"

#define CITY_WORLD_LEVELS (CITY_MAX_DRAW_ITEMS * 2u)
#define CITY_WORLD_RADIUS_PIXELS 320u

#if defined(__sh__) || defined(__SH2__)
#define CITY_LOD_WRAM CITY_WRAM_L
#else
#define CITY_LOD_WRAM
#endif

static sat_world_lod3d_object_t g_objects[CITY_MAX_DRAW_ITEMS] CITY_LOD_WRAM;
static sat_world_lod3d_level_t g_levels[CITY_WORLD_LEVELS] CITY_LOD_WRAM;
static sat_world_lod3d_state_t g_city_states[CITY_CHUNK_COUNT] CITY_LOD_WRAM;
static sat_world_lod3d_state_t g_states[CITY_MAX_DRAW_ITEMS] CITY_LOD_WRAM;
static sat_world_lod3d_draw_t g_draws[CITY_MAX_DRAW_ITEMS] CITY_LOD_WRAM;
static sat_world_lod3d_request_t g_requests[CITY_MAX_DRAW_ITEMS] CITY_LOD_WRAM;
static sat_world_lod3d_work_t g_work[CITY_MAX_DRAW_ITEMS] CITY_LOD_WRAM;

static void clear_bytes(void* value, uint32_t count) {
    uint8_t* bytes = (uint8_t*)value;
    for (uint32_t i = 0u; i < count; ++i) bytes[i] = 0u;
}

static uint8_t city_slot_lod(uint16_t slot) {
    return slot < 9u ? 0u : (slot < 34u ? 1u : 2u);
}

void city_world_lod_reset(void) {
    clear_bytes(g_city_states, sizeof(g_city_states));
}

uint32_t city_world_lod_wram_bytes(void) {
    return (uint32_t)(sizeof(g_objects) + sizeof(g_levels) + sizeof(g_city_states) +
                      sizeof(g_states) + sizeof(g_draws) + sizeof(g_requests) +
                      sizeof(g_work));
}

static void set_chunk_bounds(sat_world_lod3d_object_t* object,
                              const city_pos_t* position,
                              int32_t cx, int32_t cz,
                              sat_fx16_t eye_y,
                              sat_fx16_t world_min_y,
                              sat_fx16_t world_max_y) {
    const int32_t x0 = city_origin_rel_fx(cx, position->chunk_x) - position->local_x;
    const int32_t z0 = city_origin_rel_fx(cz, position->chunk_z) - position->local_z;
    const int32_t overhang = CITY_OVERHANG_UNITS << 16;
    object->bounds_min[0] = x0 - overhang;
    object->bounds_max[0] = x0 + CITY_CHUNK_FX + overhang;
    object->bounds_min[1] = world_min_y - eye_y;
    object->bounds_max[1] = world_max_y - eye_y;
    object->bounds_min[2] = z0 - overhang;
    object->bounds_max[2] = z0 + CITY_CHUNK_FX + overhang;
}

static void set_level(sat_world_lod3d_level_t* level,
                      const city_residency_t* residency,
                      int32_t cx, int32_t cz, uint8_t lod, int slot) {
    clear_bytes(level, sizeof(*level));
    level->asset_id = slot >= 0 ? (uint32_t)slot :
        (uint32_t)city_ring_slot(lod, cx, cz);
    level->min_projected_radius_px_16_16 = lod == 0u ? 1u : 0u;
    if (slot < 0) return;

    const city_slot_t* resident = &residency->slots[slot];
    level->faces = resident->faces;
    /* City textures are already resident in fixed, separately reserved VDP1
     * spans. Their memory is accounted by the resource plan, not per draw. */
    /* Most faces emit one command. Reserve a bounded per-face clipping
     * allowance as well; the scene reports actual commands after emission and
     * the governor can react to larger-than-estimated clip/fallback costs. */
    level->command_estimate = resident->faces + (resident->faces + 15u) / 16u;
    level->slot_generation = resident->generation;
    level->resident_generation = resident->generation;
    level->resident = 1u;
    if (resident->state == CITY_SLOT_EMPTY) {
        level->flags = SAT_WORLD_LOD3D_LEVEL_EMPTY;
        level->faces = 0u;
        level->command_estimate = 0u;
    }
}

sat_result_t city_world_plan_frame(
    const city_residency_t* residency, const city_pos_t* position,
    const sat_world_lod3d_view_t* view, sat_fx16_t eye_y,
    sat_fx16_t world_min_y, sat_fx16_t world_max_y,
    uint32_t face_budget, uint32_t command_budget,
    city_visible_fn visible, void* visible_context,
    city_plan_t* out_plan, city_lod_diagnostics_t* out_diagnostics) {
    if (residency == 0 || position == 0 || view == 0 || out_plan == 0 ||
        out_diagnostics == 0 || world_min_y > world_max_y) {
        return SAT_ERR_INVALID_ARG;
    }

    city_cell_t cells[CITY_MAX_DRAW_ITEMS];
    uint16_t cell_count = 0u;
    const int32_t radius = CITY_RING_RADIUS(CITY_LOD_COUNT - 1);
    for (int32_t dz = -radius; dz <= radius; ++dz) {
        for (int32_t dx = -radius; dx <= radius; ++dx) {
            const int32_t cx = position->chunk_x + dx;
            const int32_t cz = position->chunk_z + dz;
            if (!city_chunk_valid(cx, cz)) continue;
            cells[cell_count].cx = (int16_t)cx;
            cells[cell_count].cz = (int16_t)cz;
            cells[cell_count].dist2 = city_cell_dist2(dx, dz, position);
            ++cell_count;
        }
    }
    city_cells_sort(cells, cell_count);

    city_plan_t plan;
    city_lod_diagnostics_t diagnostics;
    clear_bytes(&plan, sizeof(plan));
    clear_bytes(&diagnostics, sizeof(diagnostics));
    uint16_t object_count = 0u;
    uint16_t level_count = 0u;
    for (uint16_t i = 0u; i < cell_count; ++i) {
        const int32_t cx = cells[i].cx;
        const int32_t cz = cells[i].cz;
        const int wanted = city_lod_for_distance(
            city_cheb(cx - position->chunk_x, cz - position->chunk_z));
        if (wanted < 0) continue;
        ++diagnostics.wanted_by_lod[wanted];

        int chosen_lod = -1;
        int chosen_slot = -1;
        for (int lod = wanted; lod < CITY_LOD_COUNT; ++lod) {
            const int slot = city_res_lookup(residency, (uint8_t)lod, cx, cz,
                                             position->chunk_x, position->chunk_z);
            if (slot >= 0) {
                chosen_lod = lod;
                chosen_slot = slot;
                break;
            }
        }
        if (chosen_lod < 0 || chosen_lod > wanted) ++plan.pop_in;
        if (chosen_slot >= 0 &&
            residency->slots[chosen_slot].state == CITY_SLOT_READY &&
            visible != 0 && !visible(visible_context, cx, cz,
                                     (uint8_t)chosen_lod, (uint16_t)chosen_slot)) {
            ++diagnostics.callback_frustum_rejected;
            continue;
        }
        /* A directly selected empty chunk has no geometry and no fallback in
         * the legacy ring policy. Keep its LOD count, but do not submit an
         * all-empty object (the generic API deliberately rejects those). */
        if (chosen_slot >= 0 && chosen_lod == wanted &&
            residency->slots[chosen_slot].state == CITY_SLOT_EMPTY) {
            continue;
        }

        sat_world_lod3d_object_t* object = &g_objects[object_count];
        clear_bytes(object, sizeof(*object));
        object->stable_id = (uint32_t)city_chunk_index(cx, cz);
        object->generation = ((uint32_t)residency->slots[
            city_ring_slot((uint8_t)wanted, cx, cz)].generation << 2u) |
            (uint32_t)wanted;
        object->levels_first = level_count;
        object->level_count = 1u;
        object->flags = SAT_WORLD_LOD3D_OBJECT_OPTIONAL |
                        SAT_WORLD_LOD3D_OBJECT_ALLOW_EMPTY_FAR;
        object->priority = (uint8_t)(cell_count - 1u - i);
        object->empty_max_projected_radius_px_16_16 =
            CITY_WORLD_RADIUS_PIXELS << 16u;
        set_chunk_bounds(object, position, cx, cz, eye_y, world_min_y, world_max_y);

        clear_bytes(&g_levels[level_count], sizeof(g_levels[level_count]));
        set_level(&g_levels[level_count], residency, cx, cz,
                  (uint8_t)wanted,
                  city_res_lookup(residency, (uint8_t)wanted, cx, cz,
                                  position->chunk_x, position->chunk_z));
        g_levels[level_count].min_projected_radius_px_16_16 = 0u;
        ++level_count;

        if (chosen_lod < 0) {
            /* Missing target: include only its first resident fallback. This
             * lets residency fallback work without allowing a second budget
             * coarsening step beyond the city's old wanted+1 limit. */
            for (int lod = wanted + 1; lod < CITY_LOD_COUNT; ++lod) {
                const int slot = city_res_lookup(residency, (uint8_t)lod, cx, cz,
                                                 position->chunk_x, position->chunk_z);
                if (slot < 0) continue;
                set_level(&g_levels[level_count], residency, cx, cz,
                          (uint8_t)lod, slot);
                g_levels[level_count].min_projected_radius_px_16_16 = 0u;
                ++level_count;
                ++object->level_count;
                break;
            }
        } else if (chosen_lod > wanted) {
            set_level(&g_levels[level_count], residency, cx, cz,
                      (uint8_t)chosen_lod, chosen_slot);
            g_levels[level_count].min_projected_radius_px_16_16 = 0u;
            ++level_count;
            ++object->level_count;
        } else if (chosen_lod == wanted &&
                   residency->slots[chosen_slot].state == CITY_SLOT_READY &&
                   wanted + 1 < CITY_LOD_COUNT) {
            /* The city baseline permits one far-ring budget step, and only
             * when the immediately coarser ring copy is already resident. */
            const int next_slot = city_res_lookup(
                residency, (uint8_t)(wanted + 1), cx, cz,
                position->chunk_x, position->chunk_z);
            if (next_slot >= 0) {
                set_level(&g_levels[level_count], residency, cx, cz,
                          (uint8_t)(wanted + 1), next_slot);
                g_levels[level_count].min_projected_radius_px_16_16 = 0u;
                ++level_count;
                ++object->level_count;
            }
        }

        const uint16_t id = (uint16_t)object->stable_id;
        g_states[object_count] = g_city_states[id];
        ++object_count;
    }

    sat_world_lod3d_view_t planning_view = *view;
    /* The cell's ring-derived LOD is encoded by the first level in each
     * per-object level span. A one-pixel sub-LSB threshold makes that first
     * level the requested variant for every in-range city chunk; the generic
     * planner still owns resident fallback, hysteresis state, budget steps,
     * deterministic ordering, and capacity validation. */
    for (uint16_t i = 0u; i < object_count; ++i) {
        g_levels[g_objects[i].levels_first].min_projected_radius_px_16_16 =
            g_objects[i].level_count == 1u ? 0u : 1u;
    }
    sat_world_lod3d_budget_t budget;
    clear_bytes(&budget, sizeof(budget));
    budget.max_faces = face_budget;
    budget.max_commands = command_budget;
    budget.max_texture_bytes = UINT32_MAX;
    budget.max_budget_lod_loss = 1u;

    sat_world_lod3d_stats_t world_stats;
    clear_bytes(&world_stats, sizeof(world_stats));
    const sat_result_t result = sat_world_lod3d_plan(
        &planning_view, &budget, g_objects, object_count, g_levels, level_count,
        g_states, g_draws, CITY_MAX_DRAW_ITEMS, g_requests, CITY_MAX_DRAW_ITEMS,
        g_work, CITY_MAX_DRAW_ITEMS, &world_stats);
    if (result != SAT_OK) {
        return result;
    }

    diagnostics.planner = world_stats;
    plan.degraded = world_stats.budget_degraded_objects;
    plan.budget_skipped = world_stats.optional_dropped;
    for (uint16_t output = 0u; output < world_stats.draw_count; ++output) {
        const sat_world_lod3d_draw_t* draw =
            &g_draws[world_stats.draw_count - 1u - output];
        const int32_t cx = (int32_t)(draw->stable_id % CITY_GRID_X);
        const int32_t cz = (int32_t)(draw->stable_id / CITY_GRID_X);
        const uint16_t slot = (uint16_t)draw->asset_id;
        if (slot >= CITY_SLOTS_TOTAL ||
            residency->slots[slot].generation != draw->residency_generation ||
            residency->slots[slot].chunk_x != cx || residency->slots[slot].chunk_z != cz) {
            ++diagnostics.planner.stale_residency_objects;
            continue;
        }
        if (residency->slots[slot].state != CITY_SLOT_READY) continue;
        city_draw_item_t* item = &plan.items[plan.count];
        item->chunk_x = (int16_t)cx;
        item->chunk_z = (int16_t)cz;
        item->slot = slot;
        item->faces = residency->slots[slot].faces;
        item->lod = city_slot_lod(slot);
        item->pass = (uint8_t)(world_stats.draw_count - 1u - output);
        item->distance = (uint16_t)city_isqrt((uint32_t)city_cell_dist2(
            cx - position->chunk_x, cz - position->chunk_z, position));
        plan.faces += item->faces;
        ++diagnostics.selected_by_lod[item->lod];
        ++plan.count;
    }
    for (uint16_t i = 0u; i < object_count; ++i) {
        const uint16_t id = (uint16_t)g_objects[i].stable_id;
        g_city_states[id] = g_states[i];
    }
    *out_plan = plan;
    *out_diagnostics = diagnostics;
    return SAT_OK;
}
