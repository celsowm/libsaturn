#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "saturn/world_lod_fade3d.h"
#include "saturn/vdp1.h"

namespace {

constexpr int32_t fx(int32_t units) { return units * 65536; }

sat_fade3d_slots_t make_fade() {
    sat_fade3d_slots_t fade = {};
    fade.policy.start = fx(10);
    fade.policy.end = fx(18);
    fade.policy.levels = 8u;
    fade.policy.flags = SAT_FADE3D_CULL_AFTER_END;
    fade.hysteresis = fx(2);
    fade.base_slot = 0u;
    fade.slot_stride = 1u;
    fade.opaque_before_start = 1u;
    return fade;
}

sat_world_lod3d_draw_t make_draw(int32_t depth) {
    sat_world_lod3d_draw_t draw = {};
    draw.stable_id = 7u;
    draw.object_generation = 3u;
    draw.camera_depth_16_16 = depth;
    draw.lod = 0u;
    draw.asset_id = 100u;
    return draw;
}

sat_world_lod3d_fade_composition_t make_composition() {
    sat_world_lod3d_fade_composition_t composition = {};
    composition.fade_enabled = 1u;
    composition.color_calc_configured = 1u;
    composition.normal_sprite_priority = 7u;
    composition.backdrop_priority = 5u;
    composition.unsupported_fallback = SAT_WORLD_LOD3D_FADE_FALLBACK_OPAQUE;
    return composition;
}

sat_scene3d_material_t make_indexed_material(sat_vdp1_texture_t* texture) {
    *texture = {};
    texture->valid = 1u;
    texture->format = SAT_VDP1_TEXTURE_INDEXED8;
    sat_scene3d_material_t material = {};
    material.kind = SAT_SCENE3D_INDEXED_SOLID;
    material.texture = texture;
    material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    return material;
}

sat_result_t apply(const sat_fade3d_slots_t* fade,
                   const sat_world_lod3d_draw_t& draw,
                   const sat_scene3d_material_t& material,
                   const sat_world_lod3d_fade_composition_t& composition,
                   uint32_t view_generation,
                   sat_world_lod3d_fade_state_t* state,
                   sat_scene3d_material_t* out_material,
                   sat_world_lod3d_fade_result_t* out_result) {
    return sat_world_lod3d_fade_apply(fade, &draw, &material, &composition,
                                      view_generation, state, out_material,
                                      out_result);
}

void indexed_material_fades_only_over_valid_lower_priority_backdrop() {
    auto fade = make_fade();
    auto draw = make_draw(fx(15));
    auto composition = make_composition();
    sat_vdp1_texture_t texture = {};
    auto material = make_indexed_material(&texture);
    sat_world_lod3d_fade_state_t state = {};
    sat_scene3d_material_t output = {};
    sat_world_lod3d_fade_result_t result = {};

    assert(apply(&fade, draw, material, composition, 9u, &state,
                 &output, &result) == SAT_OK);
    assert(result.draw == 1u && result.disposition == SAT_WORLD_LOD3D_FADE_APPLIED);
    assert(result.color_calc_slot == 5u && output.color_calc_slot == 5u);
    assert(state.valid == 1u && state.object_generation == draw.object_generation);
    assert(state.view_generation == 9u && state.color_calc_slot == 5u);

    /* The sprite color-calculation selector is priority 6, so a VDP2 layer at
     * 6 is not below it and cannot be advertised as a supported backdrop. */
    composition.backdrop_priority = 6u;
    state = {};
    assert(apply(&fade, draw, material, composition, 9u, &state,
                 &output, &result) == SAT_OK);
    assert(result.draw == 1u &&
           result.disposition == SAT_WORLD_LOD3D_FADE_OPAQUE_UNSUPPORTED);
    assert(output.color_calc_slot == SAT_INDEXED_SOLID_OPAQUE);
}

void jitter_reversal_and_lod_swap_keep_existing_fade_history() {
    auto fade = make_fade();
    auto composition = make_composition();
    sat_vdp1_texture_t texture = {};
    auto material = make_indexed_material(&texture);
    sat_world_lod3d_fade_state_t state = {};
    sat_scene3d_material_t output = {};
    sat_world_lod3d_fade_result_t result = {};
    auto draw = make_draw(fx(12) + 1); /* one LSB past the opaque band */

    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.color_calc_slot == 2u);

    /* Switching the selected mesh is not a fade-state reset. One-LSB motion
     * around the same transition remains on the established ratio slot. */
    draw.lod = 1u;
    draw.asset_id = 101u;
    draw.camera_depth_16_16 = fx(12);
    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.color_calc_slot == 2u);
    draw.camera_depth_16_16 = fx(11);
    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.color_calc_slot == 2u);

    /* Reversing far enough crosses the lower hysteresis edge and returns to
     * opaque; approaching again selects the fade slot without stale state. */
    draw.camera_depth_16_16 = fx(9);
    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.color_calc_slot == SAT_INDEXED_SOLID_OPAQUE);
    draw.camera_depth_16_16 = fx(12) + 1;
    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.color_calc_slot == 2u);
}

void object_and_view_generation_reset_teleport_history() {
    auto fade = make_fade();
    auto composition = make_composition();
    sat_vdp1_texture_t texture = {};
    auto material = make_indexed_material(&texture);
    sat_world_lod3d_fade_state_t state = {};
    sat_scene3d_material_t output = {};
    sat_world_lod3d_fade_result_t result = {};
    auto draw = make_draw(fx(18));

    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.draw == 0u && result.disposition == SAT_WORLD_LOD3D_FADE_CULLED_BY_DISTANCE);
    assert(result.color_calc_slot != SAT_FADE3D_SLOT_CULLED);

    draw.camera_depth_16_16 = fx(17);
    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.draw == 0u); /* old cull slot remains sticky */

    assert(apply(&fade, draw, material, composition, 2u, &state,
                 &output, &result) == SAT_OK);
    assert(result.draw == 1u && result.color_calc_slot == 7u); /* teleport */

    draw.object_generation += 1u;
    assert(apply(&fade, draw, material, composition, 2u, &state,
                 &output, &result) == SAT_OK);
    assert(result.draw == 1u && result.color_calc_slot == 7u); /* asset reset */
}

void unsupported_formats_use_explicit_fallback_without_culled_slot() {
    auto fade = make_fade();
    auto draw = make_draw(fx(15));
    auto composition = make_composition();
    sat_vdp1_texture_t texture = {};
    auto material = make_indexed_material(&texture);
    material.kind = SAT_SCENE3D_RGB;
    material.texture = nullptr;
    sat_world_lod3d_fade_state_t state = {};
    sat_scene3d_material_t output = {};
    sat_world_lod3d_fade_result_t result = {};

    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.disposition == SAT_WORLD_LOD3D_FADE_OPAQUE_UNSUPPORTED);
    assert(output.color_calc_slot == SAT_INDEXED_SOLID_OPAQUE);
    assert(result.color_calc_slot != SAT_FADE3D_SLOT_CULLED);

    composition.unsupported_fallback = SAT_WORLD_LOD3D_FADE_FALLBACK_CULL;
    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.draw == 0u &&
           result.disposition == SAT_WORLD_LOD3D_FADE_CULLED_UNSUPPORTED);
    assert(result.color_calc_slot == SAT_INDEXED_SOLID_OPAQUE);

    material.kind = SAT_SCENE3D_INDEXED_SOLID;
    material.texture = &texture;
    texture.format = SAT_VDP1_TEXTURE_LUT4;
    assert(apply(&fade, draw, material, composition, 1u, &state,
                 &output, &result) == SAT_OK);
    assert(result.disposition == SAT_WORLD_LOD3D_FADE_CULLED_UNSUPPORTED);
    assert(output.color_calc_slot == SAT_INDEXED_SOLID_OPAQUE);
}

void disabled_fade_is_an_opaque_reference_and_errors_are_atomic() {
    auto composition = make_composition();
    composition.fade_enabled = 0u;
    auto draw = make_draw(fx(15));
    sat_vdp1_texture_t texture = {};
    auto material = make_indexed_material(&texture);
    material.color_calc_slot = 4u;
    sat_world_lod3d_fade_state_t state = {9u, 8u, 4u, 1u, 0u};
    sat_scene3d_material_t output = {};
    sat_world_lod3d_fade_result_t result = {};
    assert(apply(nullptr, draw, material, composition, 2u, &state,
                 &output, &result) == SAT_OK);
    assert(result.draw == 1u && result.disposition == SAT_WORLD_LOD3D_FADE_DISABLED);
    assert(output.color_calc_slot == SAT_INDEXED_SOLID_OPAQUE);
    assert(state.color_calc_slot == SAT_INDEXED_SOLID_OPAQUE && state.view_generation == 2u);

    material.color_calc_slot = SAT_FADE3D_SLOT_CULLED;
    assert(apply(nullptr, draw, material, composition, 2u, &state,
                 &output, &result) == SAT_OK);
    assert(output.color_calc_slot == SAT_INDEXED_SOLID_OPAQUE &&
           result.color_calc_slot == SAT_INDEXED_SOLID_OPAQUE);
    material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;

    composition.fade_enabled = 1u;
    const auto state_before = state;
    std::memset(&output, 0xA5, sizeof(output));
    std::memset(&result, 0x5A, sizeof(result));
    const auto output_before = output;
    const auto result_before = result;
    assert(apply(nullptr, draw, material, composition, 2u, &state,
                 &output, &result) == SAT_ERR_INVALID_ARG);
    assert(std::memcmp(&state, &state_before, sizeof(state)) == 0);
    assert(std::memcmp(&output, &output_before, sizeof(output)) == 0);
    assert(std::memcmp(&result, &result_before, sizeof(result)) == 0);
}

}  // namespace

int main() {
    indexed_material_fades_only_over_valid_lower_priority_backdrop();
    jitter_reversal_and_lod_swap_keep_existing_fade_history();
    object_and_view_generation_reset_teleport_history();
    unsupported_formats_use_explicit_fallback_without_culled_slot();
    disabled_fade_is_an_opaque_reference_and_errors_are_atomic();
    std::puts("world LOD fade bridge: OK (5 test groups)");
    return 0;
}
