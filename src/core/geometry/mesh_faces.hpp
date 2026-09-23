#ifndef SATURN_CORE_GEOMETRY_MESH_FACES_HPP
#define SATURN_CORE_GEOMETRY_MESH_FACES_HPP

#include "saturn/mesh3d.h"
#include "src/core/math3d/logic.hpp"

/* Shared, allocation-free mesh/quad queries. Neither collision nor the
 * mesh renderer owns the algorithm: both consume these independent helpers.
 * The public sat_mesh_t layout remains borrowed from the public C API. */
namespace saturn::core::geometry {
using saturn::core::math3d::vec3_cross_scaled;
using saturn::core::math3d::vec3_sub;

inline const uint16_t* face_indices(const sat_mesh_t* mesh, uint16_t face) {
    if (mesh == nullptr || mesh->indices == nullptr || face >= mesh->face_count) {
        return nullptr;
    }
    return &mesh->indices[static_cast<uint32_t>(face) * 4u];
}

inline sat_result_t face_quad(const sat_mesh_t* mesh, uint16_t face, sat_quad3_t* out) {
    const uint16_t* idx = face_indices(mesh, face);
    if (idx == nullptr || out == nullptr || mesh->vertices == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 4; ++i) {
        if (idx[i] >= mesh->vertex_count) return SAT_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 4; ++i) {
        out->v[i] = mesh->vertices[idx[i]];
    }
    return SAT_OK;
}


/* Outward normal WITHOUT normalising: direction is right, length is not.
 *
 * This is the hot path. Backface culling only needs the sign of a dot
 * product, and shading only needs the dot divided by the length, so making
 * every face pay for a square root and three 64-bit divides -- which is what
 * a normalised normal costs on a CPU with neither -- is pure waste. Profiling
 * the board camera put 64% of the frame in exactly that.
 *
 * Degenerate faces are ordinary here, not exceptional: a sphere pole band
 * repeats A and B, a cylinder cap repeats A and D. Substituting C for the
 * collapsed corner gives a vector that turns the same way, so the sign of the
 * normal survives; taking the opposite diagonal instead does not, which flips
 * every pole face inward. */
inline sat_vec3_t quad_normal_scaled(const sat_quad3_t& quad) {
    sat_vec3_t n = vec3_cross_scaled(
        vec3_sub(quad.v[3], quad.v[0]), vec3_sub(quad.v[1], quad.v[0]));
    if (n.x == 0 && n.y == 0 && n.z == 0) {
        /* B collapsed onto A: the face is the triangle A, C, D. */
        n = vec3_cross_scaled(
            vec3_sub(quad.v[3], quad.v[0]), vec3_sub(quad.v[2], quad.v[0]));
    }
    if (n.x == 0 && n.y == 0 && n.z == 0) {
        /* D collapsed onto A: the face is the triangle A, B, C. */
        n = vec3_cross_scaled(
            vec3_sub(quad.v[2], quad.v[0]), vec3_sub(quad.v[1], quad.v[0]));
    }
    return n;
}


} // namespace saturn::core::geometry
#endif /* SATURN_CORE_GEOMETRY_MESH_FACES_HPP */
