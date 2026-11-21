#include "saturn/water_actor.h"

namespace {

bool takes_light(const sat_scene3d_material_t& m) {
    return m.kind == SAT_SCENE3D_RGB || m.kind == SAT_SCENE3D_INDEXED_TEXTURED;
}

sat_result_t submit_side(sat_scene_t* scene, const sat_mesh_t* mesh, uint16_t* indices,
    uint16_t* face_materials, uint16_t faces, const sat_scene3d_material_t* materials,
    uint16_t material_count, const uint16_t* light, uint8_t cull, uint16_t pass,
    sat_projected_vertex_t* scratch) {
    if (faces == 0) return SAT_OK;
    sat_scene3d_material_t local[SAT_WATER_ACTOR_MATERIALS_MAX];
    for (uint16_t m = 0; m < material_count; ++m) {
        local[m] = materials[m];
        if (light && takes_light(local[m])) local[m].vertex_gouraud = light;
    }
    sat_mesh_t part = *mesh;
    part.indices = indices;
    part.face_count = faces;
    part.face_cap = faces;
    sat_scene3d_instance_t instance{};
    instance.mesh = &part;
    instance.materials = local;
    instance.material_count = material_count;
    instance.face_materials = face_materials;
    instance.pass = pass;
    instance.cull_backfaces = cull;
    return sat_scene_submit_instance(scene, &instance, SAT_SCENE3D_SLOT_INHERIT, scratch, nullptr);
}

}

extern "C" sat_result_t sat_water_actor_submit(sat_scene_t* scene, const sat_mesh_t* mesh,
    const sat_scene3d_material_t* materials, uint16_t material_count,
    const uint16_t* base_light, uint8_t cull_backfaces,
    const sat_water_actor_params_t* params, sat_water_actor_workspace_t* ws,
    const sat_water_actor_split_t* split) {
    if (!scene || !mesh || !materials || material_count == 0 ||
        material_count > SAT_WATER_ACTOR_MATERIALS_MAX || !params || !ws || !ws->scratch || !split)
        return SAT_ERR_INVALID_ARG;
    sat_result_t result = submit_side(scene, mesh, ws->above_indices, ws->above_materials,
        split->above_faces, materials, material_count, base_light, cull_backfaces,
        params->pass_above, ws->scratch);
    if (result != SAT_OK) return result;
    result = submit_side(scene, mesh, ws->below_indices, ws->below_materials,
        split->below_faces, materials, material_count, ws->wet_light, cull_backfaces,
        params->pass_below, ws->scratch);
    if (result != SAT_OK) return result;
    for (uint16_t p = 0; p < split->pieces; ++p) {
        sat_scene3d_material_t material = materials[ws->piece_materials[p]];
        material.vertex_gouraud = ws->pieces[p].color;
        result = sat_scene_submit_quad(scene, &ws->pieces[p].piece.quad, &material,
            ws->pieces[p].piece.submerged ? params->pass_below : params->pass_above);
        if (result != SAT_OK) return result;
    }
    return SAT_OK;
}
