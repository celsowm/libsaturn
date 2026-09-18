#ifndef SATURN_CORE_MESH3D_COLLISION_GRID_HPP
#define SATURN_CORE_MESH3D_COLLISION_GRID_HPP

#include <stdint.h>

#include "saturn/collide3d.h"
#include "src/core/collide3d_logic.hpp"
#include "src/core/spatial3_hash.hpp"

namespace saturn::core::collide3d {

inline void quad_bounds_raw(
    const sat_quad3_t& q,
    int64_t& minx, int64_t& miny, int64_t& minz,
    int64_t& maxx, int64_t& maxy, int64_t& maxz
) {
    minx = maxx = q.v[0].x;
    miny = maxy = q.v[0].y;
    minz = maxz = q.v[0].z;
    for (int i = 1; i < 4; ++i) {
        if (q.v[i].x < minx) minx = q.v[i].x;
        if (q.v[i].x > maxx) maxx = q.v[i].x;
        if (q.v[i].y < miny) miny = q.v[i].y;
        if (q.v[i].y > maxy) maxy = q.v[i].y;
        if (q.v[i].z < minz) minz = q.v[i].z;
        if (q.v[i].z > maxz) maxz = q.v[i].z;
    }
}

inline void mesh3_grid_face_cells(
    const sat_quad3_t& q,
    uint8_t shift,
    int32_t& x0, int32_t& y0, int32_t& z0,
    int32_t& x1, int32_t& y1, int32_t& z1
) {
    int64_t minx, miny, minz, maxx, maxy, maxz;
    quad_bounds_raw(q, minx, miny, minz, maxx, maxy, maxz);
    x0 = saturn::core::spatial3_hash::cell(minx, shift);
    y0 = saturn::core::spatial3_hash::cell(miny, shift);
    z0 = saturn::core::spatial3_hash::cell(minz, shift);
    x1 = saturn::core::spatial3_hash::cell(maxx, shift);
    y1 = saturn::core::spatial3_hash::cell(maxy, shift);
    z1 = saturn::core::spatial3_hash::cell(maxz, shift);
}

inline bool mesh3_grid_valid(const sat_mesh3_grid_t* grid) {
    return grid != nullptr && grid->mesh != nullptr && grid->heads != nullptr &&
           grid->entries != nullptr && grid->stamps != nullptr &&
           saturn::core::spatial3_hash::power_of_two(grid->bucket_count) &&
           grid->stamp_cap >= grid->mesh->face_count;
}

inline sat_result_t mesh3_grid_init(
    sat_mesh3_grid_t& grid,
    const sat_mesh_t& mesh,
    uint8_t cell_shift,
    uint16_t* heads,
    uint16_t bucket_count,
    sat_mesh3_grid_entry_t* entries,
    uint16_t entry_cap,
    uint16_t* stamps,
    uint16_t stamp_cap
) {
    if (heads == nullptr || entries == nullptr || stamps == nullptr ||
        mesh.vertices == nullptr || mesh.indices == nullptr ||
        mesh.face_count == 0u || entry_cap == 0u ||
        !saturn::core::spatial3_hash::power_of_two(bucket_count) ||
        cell_shift > 15u || stamp_cap < mesh.face_count) {
        return SAT_ERR_INVALID_ARG;
    }

    uint64_t required = 0u;
    for (uint16_t face = 0; face < mesh.face_count; ++face) {
        sat_quad3_t q;
        if (saturn::core::mesh3d::face_quad(&mesh, face, &q) != SAT_OK) {
            return SAT_ERR_INVALID_ARG;
        }
        int32_t x0, y0, z0, x1, y1, z1;
        mesh3_grid_face_cells(q, cell_shift, x0, y0, z0, x1, y1, z1);
        required += saturn::core::spatial3_hash::cell_count(
            x0, y0, z0, x1, y1, z1);
        if (required > entry_cap) return SAT_ERR_CAPACITY;
    }

    grid.mesh = &mesh;
    grid.heads = heads;
    grid.entries = entries;
    grid.stamps = stamps;
    grid.bucket_count = bucket_count;
    grid.entry_cap = entry_cap;
    grid.entry_count = 0u;
    grid.stamp_cap = stamp_cap;
    grid.query_stamp = 0u;
    grid.cell_shift = cell_shift;
    for (uint16_t i = 0; i < bucket_count; ++i) heads[i] = SAT_MESH3_GRID_EMPTY;
    for (uint16_t i = 0; i < mesh.face_count; ++i) stamps[i] = 0u;

    for (uint16_t face = 0; face < mesh.face_count; ++face) {
        sat_quad3_t q;
        (void)saturn::core::mesh3d::face_quad(&mesh, face, &q);
        int32_t x0, y0, z0, x1, y1, z1;
        mesh3_grid_face_cells(q, cell_shift, x0, y0, z0, x1, y1, z1);
        for (int32_t z = z0; z <= z1; ++z) {
            for (int32_t y = y0; y <= y1; ++y) {
                for (int32_t x = x0; x <= x1; ++x) {
                    const uint16_t bucket = saturn::core::spatial3_hash::bucket(
                        grid.bucket_count, x, y, z);
                    sat_mesh3_grid_entry_t& e = entries[grid.entry_count];
                    e.cell_x = x;
                    e.cell_y = y;
                    e.cell_z = z;
                    e.face = face;
                    e.next = heads[bucket];
                    heads[bucket] = grid.entry_count++;
                }
            }
        }
    }
    return SAT_OK;
}

inline sat_result_t sphere_mesh_grid(
    sat_mesh3_grid_t& grid,
    const sat_sphere_t& sphere,
    sat_contact3_t* out,
    uint16_t cap,
    uint16_t& count
) {
    if (!mesh3_grid_valid(&grid) || sphere.radius < 0 || (out == nullptr && cap != 0u)) {
        return SAT_ERR_INVALID_ARG;
    }
    count = 0u;
    ++grid.query_stamp;
    if (grid.query_stamp == 0u) {
        for (uint16_t i = 0; i < grid.mesh->face_count; ++i) grid.stamps[i] = 0u;
        grid.query_stamp = 1u;
    }

    const int32_t x0 = saturn::core::spatial3_hash::cell(
        static_cast<int64_t>(sphere.center.x) - sphere.radius, grid.cell_shift);
    const int32_t y0 = saturn::core::spatial3_hash::cell(
        static_cast<int64_t>(sphere.center.y) - sphere.radius, grid.cell_shift);
    const int32_t z0 = saturn::core::spatial3_hash::cell(
        static_cast<int64_t>(sphere.center.z) - sphere.radius, grid.cell_shift);
    const int32_t x1 = saturn::core::spatial3_hash::cell(
        static_cast<int64_t>(sphere.center.x) + sphere.radius, grid.cell_shift);
    const int32_t y1 = saturn::core::spatial3_hash::cell(
        static_cast<int64_t>(sphere.center.y) + sphere.radius, grid.cell_shift);
    const int32_t z1 = saturn::core::spatial3_hash::cell(
        static_cast<int64_t>(sphere.center.z) + sphere.radius, grid.cell_shift);

    sat_result_t result = SAT_OK;
    for (int32_t z = z0; z <= z1; ++z) {
        for (int32_t y = y0; y <= y1; ++y) {
            for (int32_t x = x0; x <= x1; ++x) {
                const uint16_t bucket = saturn::core::spatial3_hash::bucket(
                    grid.bucket_count, x, y, z);
                for (uint16_t ei = grid.heads[bucket];
                     ei != SAT_MESH3_GRID_EMPTY;
                     ei = grid.entries[ei].next) {
                    const sat_mesh3_grid_entry_t& e = grid.entries[ei];
                    if (e.cell_x != x || e.cell_y != y || e.cell_z != z) continue;
                    if (grid.stamps[e.face] == grid.query_stamp) continue;
                    grid.stamps[e.face] = grid.query_stamp;

                    sat_quad3_t q;
                    if (saturn::core::mesh3d::face_quad(grid.mesh, e.face, &q) != SAT_OK) continue;
                    sat_contact3_t c;
                    if (!sphere_quad_contact(q, sphere, c)) continue;
                    if (count >= cap) {
                        result = SAT_ERR_CAPACITY;
                        continue;
                    }
                    out[count++] = c;
                }
            }
        }
    }
    return result;
}

}  // namespace saturn::core::collide3d

#endif /* SATURN_CORE_MESH3D_COLLISION_GRID_HPP */
