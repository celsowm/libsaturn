#include "saturn/water_prepare.h"

namespace {

constexpr uint16_t kNeutral = 0x4210u;

struct Context {
    const sat_water_prepare_config_t& config;
    sat_water_prepared_t& out;
    uint16_t below_next;  // faces are written downward from the end for below
};

int channel(uint16_t entry, unsigned shift) { return (entry >> shift) & 31; }

uint16_t tinted(const sat_water_prepare_config_t& c, uint16_t base, int64_t depth) {
    if (depth <= 0) return base;
    const int64_t t = depth >= c.full_depth ? SAT_FX16_ONE : depth * SAT_FX16_ONE / c.full_depth;
    int out[3];
    for (unsigned k = 0; k < 3; ++k) {
        const int v = channel(base, 5 * k) + static_cast<int>(c.tint[k] * t / SAT_FX16_ONE);
        out[k] = v < 0 ? 0 : (v > 31 ? 31 : v);
    }
    return static_cast<uint16_t>((out[2] << 10) | (out[1] << 5) | out[0]);
}

/* The surface of the region over `p` (x, z and the lowest y), or false. */
bool surface_over(const sat_water_regions_t* regions, const sat_vec3_t& p, sat_fx16_t& surface) {
    uint16_t index = SAT_WATER_REGION_NONE;
    if (sat_water_regions_find(regions, &p, &index) != SAT_OK || index == SAT_WATER_REGION_NONE)
        return false;
    surface = regions->volumes[index].surface;
    return true;
}

}

extern "C" sat_result_t sat_water_prepare_mesh(const sat_mesh_t* mesh,
    const uint16_t* face_materials, const sat_scene3d_material_t* materials,
    uint16_t material_count, const uint16_t* base_gouraud,
    const sat_water_prepare_config_t* config, sat_water_prepared_t* io) {
    if (!mesh || !mesh->vertices || !mesh->indices || !materials || material_count == 0 ||
        !config || !config->regions || config->full_depth <= 0 || !io ||
        !io->vertices || !io->gouraud || !io->indices || !io->face_materials)
        return SAT_ERR_INVALID_ARG;
    if (mesh->vertex_count > io->vertex_cap || mesh->face_count > io->face_cap)
        return SAT_ERR_CAPACITY;
    io->vertex_count = mesh->vertex_count;
    io->above_faces = io->below_faces = io->cut_faces = io->uncut_faces = 0;

    for (uint16_t v = 0; v < mesh->vertex_count; ++v) {
        const sat_vec3_t& p = mesh->vertices[v];
        io->vertices[v] = p;
        const uint16_t base = base_gouraud ? base_gouraud[v] : kNeutral;
        sat_fx16_t surface = 0;
        io->gouraud[v] = surface_over(config->regions, p, surface)
            ? tinted(*config, base, static_cast<int64_t>(surface) - p.y) : base;
    }

    uint16_t above = 0, below_next = io->face_cap;  // below grows down from the end
    auto emit = [&](const uint16_t corner[4], uint16_t material, bool is_below) -> bool {
        if (above + (io->face_cap - below_next) >= io->face_cap) return false;
        const uint16_t slot = is_below ? static_cast<uint16_t>(--below_next) : above++;
        for (unsigned c = 0; c < 4; ++c) io->indices[4u * slot + c] = corner[c];
        io->face_materials[slot] = material;
        return true;
    };

    for (uint16_t f = 0; f < mesh->face_count; ++f) {
        const uint16_t* idx = &mesh->indices[4u * f];
        const uint16_t material = face_materials ? face_materials[f] : 0;
        if (material >= material_count) return SAT_ERR_INVALID_ARG;
        sat_quad3_t quad;
        int64_t cx = 0, cz = 0;
        sat_fx16_t low = INT32_MAX, high = INT32_MIN;
        for (unsigned c = 0; c < 4; ++c) {
            if (idx[c] >= mesh->vertex_count) return SAT_ERR_INVALID_ARG;
            quad.v[c] = mesh->vertices[idx[c]];
            cx += quad.v[c].x;
            cz += quad.v[c].z;
            if (quad.v[c].y < low) low = quad.v[c].y;
            if (quad.v[c].y > high) high = quad.v[c].y;
        }
        const sat_vec3_t centre = {static_cast<sat_fx16_t>(cx / 4), low, static_cast<sat_fx16_t>(cz / 4)};
        sat_fx16_t surface = 0;
        bool placed = false;
        if (!surface_over(config->regions, centre, surface) || low >= surface) {
            placed = emit(idx, material, false);
        } else if (high <= surface) {
            placed = emit(idx, material, true);
        } else if (materials[material].kind == SAT_SCENE3D_RGB) {
            uint16_t colors[4];
            for (unsigned c = 0; c < 4; ++c) colors[c] = base_gouraud ? base_gouraud[idx[c]] : kNeutral;
            sat_water_shaded_piece_t pieces[SAT_WATER_SPLIT_PIECES_MAX];
            uint8_t count = 0;
            const sat_result_t split = sat_water_split_quad_shaded(&quad, colors, surface, pieces, &count);
            if (split != SAT_OK) return split;
            placed = true;
            for (uint8_t p = 0; p < count && placed; ++p) {
                if (io->vertex_count + 4u > io->vertex_cap) return SAT_ERR_CAPACITY;
                uint16_t corner[4];
                for (unsigned c = 0; c < 4; ++c) {
                    const uint16_t v = io->vertex_count++;
                    const sat_vec3_t& p3 = pieces[p].piece.quad.v[c];
                    io->vertices[v] = p3;
                    io->gouraud[v] = tinted(*config, pieces[p].color[c], static_cast<int64_t>(surface) - p3.y);
                    corner[c] = v;
                }
                placed = emit(corner, material, pieces[p].piece.submerged != 0);
            }
            ++io->cut_faces;
        } else {
            ++io->uncut_faces;
            // Joins whichever side holds its centroid.
            const int64_t mid = (static_cast<int64_t>(low) + high) / 2;
            placed = emit(idx, material, mid < surface);
        }
        if (!placed) return SAT_ERR_CAPACITY;
    }

    io->above_faces = above;
    io->below_faces = static_cast<uint16_t>(io->face_cap - below_next);
    // Close the gap between the lists: the below faces, written from the end
    // downward, move up behind the above ones. The destination never lies
    // past the source, so an ascending copy is safe; the order within a list
    // carries no meaning (the painter sorts by depth).
    for (uint16_t i = 0; i < io->below_faces; ++i) {
        const uint16_t from = static_cast<uint16_t>(below_next + i);
        const uint16_t to = static_cast<uint16_t>(above + i);
        if (from == to) continue;
        for (unsigned c = 0; c < 4; ++c) io->indices[4u * to + c] = io->indices[4u * from + c];
        io->face_materials[to] = io->face_materials[from];
    }
    return SAT_OK;
}
