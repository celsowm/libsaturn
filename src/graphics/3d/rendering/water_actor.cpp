#include "saturn/water_actor.h"

namespace {

constexpr uint16_t kNeutral = 0x4210u;

int channel(uint16_t entry, unsigned shift) { return (entry >> shift) & 31; }

uint16_t tinted(const sat_water_actor_params_t& p, uint16_t base, int64_t depth) {
    if (depth <= 0) return base;
    const int64_t t = depth >= p.full_depth ? SAT_FX16_ONE : depth * SAT_FX16_ONE / p.full_depth;
    int out[3];
    for (unsigned k = 0; k < 3; ++k) {
        const int v = channel(base, 5 * k) + static_cast<int>(p.tint[k] * t / SAT_FX16_ONE);
        out[k] = v < 0 ? 0 : (v > 31 ? 31 : v);
    }
    return static_cast<uint16_t>((out[2] << 10) | (out[1] << 5) | out[0]);
}

}

extern "C" sat_result_t sat_water_actor_split(const sat_mesh_t* mesh,
    const uint16_t* face_materials, const sat_scene3d_material_t* materials,
    uint16_t material_count, const uint16_t* base_light,
    const sat_water_actor_params_t* params, sat_water_actor_workspace_t* ws,
    sat_water_actor_split_t* out) {
    if (!mesh || !mesh->vertices || !mesh->indices || !materials || material_count == 0 ||
        material_count > SAT_WATER_ACTOR_MATERIALS_MAX || !params || params->full_depth <= 0 ||
        !ws || !ws->above_indices || !ws->above_materials || !ws->below_indices ||
        !ws->below_materials || !ws->wet_light || !ws->pieces || !ws->piece_materials || !out)
        return SAT_ERR_INVALID_ARG;
    if (mesh->face_count > ws->face_cap || mesh->vertex_count > ws->vertex_cap)
        return SAT_ERR_CAPACITY;
    const sat_fx16_t surface = params->surface;
    *out = {};

    for (uint16_t v = 0; v < mesh->vertex_count; ++v) {
        const uint16_t base = base_light ? base_light[v] : kNeutral;
        ws->wet_light[v] = tinted(*params, base, static_cast<int64_t>(surface) - mesh->vertices[v].y);
    }

    for (uint16_t f = 0; f < mesh->face_count; ++f) {
        const uint16_t* idx = &mesh->indices[4u * f];
        const uint16_t material = face_materials ? face_materials[f] : 0;
        if (material >= material_count) return SAT_ERR_INVALID_ARG;
        sat_quad3_t quad;
        sat_fx16_t low = INT32_MAX, high = INT32_MIN;
        for (unsigned c = 0; c < 4; ++c) {
            if (idx[c] >= mesh->vertex_count) return SAT_ERR_INVALID_ARG;
            quad.v[c] = mesh->vertices[idx[c]];
            if (quad.v[c].y < low) low = quad.v[c].y;
            if (quad.v[c].y > high) high = quad.v[c].y;
        }
        bool below;
        if (low >= surface) {
            below = false;
        } else if (high <= surface) {
            below = true;
        } else if (materials[material].kind == SAT_SCENE3D_RGB) {
            uint16_t colors[4];
            for (unsigned c = 0; c < 4; ++c) colors[c] = base_light ? base_light[idx[c]] : kNeutral;
            sat_water_shaded_piece_t cut[SAT_WATER_SPLIT_PIECES_MAX];
            uint8_t count = 0;
            const sat_result_t split = sat_water_split_quad_shaded(&quad, colors, surface, cut, &count);
            if (split != SAT_OK) return split;
            if (out->pieces + count > ws->piece_cap) return SAT_ERR_CAPACITY;
            for (uint8_t p = 0; p < count; ++p) {
                for (unsigned c = 0; c < 4; ++c)
                    cut[p].color[c] = tinted(*params, cut[p].color[c],
                        static_cast<int64_t>(surface) - cut[p].piece.quad.v[c].y);
                ws->pieces[out->pieces] = cut[p];
                ws->piece_materials[out->pieces++] = material;
            }
            continue;
        } else {
            ++out->uncut_faces;
            below = (static_cast<int64_t>(low) + high) / 2 < surface;
        }
        uint16_t* indices = below ? ws->below_indices : ws->above_indices;
        uint16_t* mats = below ? ws->below_materials : ws->above_materials;
        uint16_t& count = below ? out->below_faces : out->above_faces;
        for (unsigned c = 0; c < 4; ++c) indices[4u * count + c] = idx[c];
        mats[count++] = material;
    }
    return SAT_OK;
}
