// Reads a mesh, water regions and a tint from stdin and prints what
// sat_water_prepare_mesh makes of them, for tests/tools/test_water_prepare_parity.py.
// Input: R regions (min_x max_x min_z max_z bottom surface), tint r g b full_depth,
// M material kinds, V vertices (x y z gouraud), F faces (i0 i1 i2 i3 material).
#include <cstdio>
#include <vector>
#include "saturn/water_prepare.h"

int main() {
    unsigned regions_n = 0, materials_n = 0, vertices_n = 0, faces_n = 0;
    if (std::scanf("%u", &regions_n) != 1) return 2;
    std::vector<sat_water_volume_t> volumes(regions_n);
    for (auto& v : volumes) {
        v = {};
        if (std::scanf("%d %d %d %d %d %d", &v.min_x, &v.max_x, &v.min_z, &v.max_z, &v.bottom, &v.surface) != 6) return 2;
        v.drag = SAT_FX16_ONE / 5;
    }
    sat_water_prepare_config_t config{};
    int tint[3];
    if (std::scanf("%d %d %d %d", &tint[0], &tint[1], &tint[2], &config.full_depth) != 4) return 2;
    for (int k = 0; k < 3; ++k) config.tint[k] = static_cast<int16_t>(tint[k]);
    if (std::scanf("%u", &materials_n) != 1) return 2;
    std::vector<sat_scene3d_material_t> materials(materials_n);
    for (auto& m : materials) {
        int kind;
        if (std::scanf("%d", &kind) != 1) return 2;
        m = {};
        m.kind = static_cast<sat_scene3d_material_kind_t>(kind);
    }
    if (std::scanf("%u", &vertices_n) != 1) return 2;
    std::vector<sat_vec3_t> vertices(vertices_n);
    std::vector<uint16_t> light(vertices_n);
    for (unsigned i = 0; i < vertices_n; ++i) {
        unsigned g;
        if (std::scanf("%d %d %d %u", &vertices[i].x, &vertices[i].y, &vertices[i].z, &g) != 4) return 2;
        light[i] = static_cast<uint16_t>(g);
    }
    if (std::scanf("%u", &faces_n) != 1) return 2;
    std::vector<uint16_t> indices(4 * faces_n), face_materials(faces_n);
    for (unsigned f = 0; f < faces_n; ++f) {
        unsigned a, b, c, d, m;
        if (std::scanf("%u %u %u %u %u", &a, &b, &c, &d, &m) != 5) return 2;
        indices[4 * f] = a; indices[4 * f + 1] = b; indices[4 * f + 2] = c; indices[4 * f + 3] = d;
        face_materials[f] = m;
    }

    sat_water_regions_t regions;
    if (sat_water_regions_init(&regions, volumes.data(), static_cast<uint16_t>(regions_n)) != SAT_OK) return 3;
    config.regions = &regions;
    sat_mesh_t mesh{vertices.data(), indices.data(), static_cast<uint16_t>(vertices_n),
                    static_cast<uint16_t>(vertices_n), static_cast<uint16_t>(faces_n),
                    static_cast<uint16_t>(faces_n)};
    const uint16_t vertex_cap = 60000, face_cap = 20000;
    std::vector<sat_vec3_t> out_vertices(vertex_cap);
    std::vector<uint16_t> out_light(vertex_cap), out_indices(4u * face_cap), out_materials(face_cap);
    sat_water_prepared_t io{};
    io.vertices = out_vertices.data(); io.gouraud = out_light.data(); io.indices = out_indices.data();
    io.face_materials = out_materials.data(); io.vertex_cap = vertex_cap; io.face_cap = face_cap;
    if (sat_water_prepare_mesh(&mesh, face_materials.data(), materials.data(),
                               static_cast<uint16_t>(materials_n), light.data(), &config, &io) != SAT_OK) return 4;
    std::printf("C %u %u %u %u\n", io.above_faces, io.below_faces, io.cut_faces, io.uncut_faces);
    std::printf("V %u\n", io.vertex_count);
    for (unsigned v = 0; v < io.vertex_count; ++v)
        std::printf("%d %d %d %u\n", out_vertices[v].x, out_vertices[v].y, out_vertices[v].z, out_light[v]);
    const unsigned faces = io.above_faces + io.below_faces;
    std::printf("F %u\n", faces);
    for (unsigned f = 0; f < faces; ++f)
        std::printf("%u %u %u %u %u\n", out_indices[4 * f], out_indices[4 * f + 1], out_indices[4 * f + 2],
                    out_indices[4 * f + 3], out_materials[f]);
    return 0;
}
