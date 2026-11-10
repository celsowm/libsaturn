#include "saturn/world_lod_fade3d.h"

#include "saturn/vdp1.h"

namespace {

bool indexed8_texture(const sat_vdp1_texture_t* texture) {
    return texture != nullptr && texture->valid != 0u &&
           texture->format == SAT_VDP1_TEXTURE_INDEXED8;
}

bool fade_capable_material(const sat_scene3d_material_t& material) {
    if (material.vertex_gouraud != nullptr) return false;
    switch (material.kind) {
        case SAT_SCENE3D_INDEXED_SOLID:
        case SAT_SCENE3D_INDEXED_TEXTURED:
            return indexed8_texture(material.texture);
        case SAT_SCENE3D_INDEXED_TILED: {
            if (material.tiled == nullptr ||
                !indexed8_texture(material.tiled->full)) return false;
            if (material.tiled->grid == 0u) {
                for (uint8_t i = 0u; i < 4u; ++i)
                    if (!indexed8_texture(material.tiled->tiles[i])) return false;
                return true;
            }
            if (material.tiled->grid != 2u && material.tiled->grid != 4u &&
                material.tiled->grid != 8u) return false;
            if (material.tiled->cells == nullptr) return false;
            const uint16_t count = static_cast<uint16_t>(material.tiled->grid) *
                                   material.tiled->grid;
            for (uint16_t i = 0u; i < count; ++i)
                if (!indexed8_texture(&material.tiled->cells[i])) return false;
            return true;
        }
        case SAT_SCENE3D_RGB:
        default:
            return false;
    }
}

bool valid_composition(const sat_world_lod3d_fade_composition_t& composition) {
    if (composition.fade_enabled > 1u ||
        composition.color_calc_configured > 1u ||
        composition.unsupported_fallback > SAT_WORLD_LOD3D_FADE_FALLBACK_CULL)
        return false;
    if (composition.fade_enabled == 0u) return true;
    return composition.normal_sprite_priority <= 7u &&
           composition.backdrop_priority <= 7u;
}

bool composition_supports_fade(
    const sat_world_lod3d_fade_composition_t& composition) {
    if (composition.fade_enabled == 0u ||
        composition.color_calc_configured == 0u ||
        composition.normal_sprite_priority < 2u) return false;
    const uint8_t fade_priority = static_cast<uint8_t>(
        composition.normal_sprite_priority - 1u);
    return composition.backdrop_priority != 0u &&
           composition.backdrop_priority < fade_priority;
}

void commit_state(sat_world_lod3d_fade_state_t* state,
                  const sat_world_lod3d_draw_t& draw,
                  uint32_t view_generation, uint8_t slot) {
    if (state == nullptr) return;
    state->object_generation = draw.object_generation;
    state->view_generation = view_generation;
    state->color_calc_slot = slot;
    state->valid = 1u;
    state->reserved = 0u;
}

}  // namespace

extern "C" sat_result_t sat_world_lod3d_fade_apply(
    const sat_fade3d_slots_t* fade,
    const sat_world_lod3d_draw_t* draw,
    const sat_scene3d_material_t* material,
    const sat_world_lod3d_fade_composition_t* composition,
    uint32_t view_generation,
    sat_world_lod3d_fade_state_t* state,
    sat_scene3d_material_t* out_material,
    sat_world_lod3d_fade_result_t* out_result) {
    if (draw == nullptr || material == nullptr || composition == nullptr ||
        out_material == nullptr || out_result == nullptr ||
        material->kind < SAT_SCENE3D_RGB ||
        material->kind > SAT_SCENE3D_INDEXED_TILED ||
        !valid_composition(*composition) || (state != nullptr && state->valid > 1u) ||
        (composition->fade_enabled != 0u && fade == nullptr)) {
        return SAT_ERR_INVALID_ARG;
    }

    sat_scene3d_material_t next_material = *material;
    sat_world_lod3d_fade_result_t next_result = {};
    next_result.draw = 1u;
    next_result.color_calc_slot = material->color_calc_slot;
    uint8_t next_state_slot = SAT_INDEXED_SOLID_OPAQUE;

    if (composition->fade_enabled == 0u) {
        next_result.disposition = SAT_WORLD_LOD3D_FADE_DISABLED;
        if (next_material.color_calc_slot < 8u ||
            next_material.color_calc_slot == SAT_FADE3D_SLOT_CULLED) {
            next_material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
            next_result.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
        }
        commit_state(state, *draw, view_generation, SAT_INDEXED_SOLID_OPAQUE);
        *out_material = next_material;
        *out_result = next_result;
        return SAT_OK;
    }

    uint8_t previous = SAT_INDEXED_SOLID_OPAQUE;
    if (state != nullptr && state->valid != 0u &&
        state->object_generation == draw->object_generation &&
        state->view_generation == view_generation) {
        previous = state->color_calc_slot;
    }
    uint8_t slot = SAT_INDEXED_SOLID_OPAQUE;
    sat_result_t status = sat_fade3d_slot(
        fade, draw->camera_depth_16_16, &previous, &slot);
    if (status != SAT_OK) return status;
    next_state_slot = previous;

    if (!fade_capable_material(*material) ||
        !composition_supports_fade(*composition)) {
        next_material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
        next_result.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
        next_result.disposition =
            composition->unsupported_fallback == SAT_WORLD_LOD3D_FADE_FALLBACK_CULL
                ? SAT_WORLD_LOD3D_FADE_CULLED_UNSUPPORTED
                : SAT_WORLD_LOD3D_FADE_OPAQUE_UNSUPPORTED;
        next_result.draw = composition->unsupported_fallback ==
                           SAT_WORLD_LOD3D_FADE_FALLBACK_CULL ? 0u : 1u;
        next_state_slot = SAT_INDEXED_SOLID_OPAQUE;
    } else if (slot == SAT_FADE3D_SLOT_CULLED) {
        next_material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
        next_result.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
        next_result.disposition = SAT_WORLD_LOD3D_FADE_CULLED_BY_DISTANCE;
        next_result.draw = 0u;
    } else {
        next_material.color_calc_slot = slot;
        next_result.color_calc_slot = slot;
        next_result.disposition = SAT_WORLD_LOD3D_FADE_APPLIED;
    }

    commit_state(state, *draw, view_generation, next_state_slot);
    *out_material = next_material;
    *out_result = next_result;
    return SAT_OK;
}
