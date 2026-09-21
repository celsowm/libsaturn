#ifndef SATURN_CORE_MESH3D_LOGIC_HPP
#define SATURN_CORE_MESH3D_LOGIC_HPP

/* Pure, host-testable quad-mesh construction.
 *
 * No hardware access, so tests/host/test_mesh3d_logic.cpp links this
 * directly. Only sat_vdp1_draw_mesh, in mesh3d_api.cpp, touches the VDP1.
 *
 * The winding contract is stated in include/saturn/mesh3d.h: corners run
 * A, B, C, D as a VDP1 quad does, and the outward normal is
 * cross(D - A, B - A). Every builder below is written to satisfy that, and
 * the host tests check it the only way that is actually convincing -- by
 * confirming each face normal points away from the solid's centre.
 */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/mesh3d.h"
#include "src/core/math3d_logic.hpp"

namespace saturn::core::mesh3d {

using saturn::core::math3d::cos_deg_fx;
using saturn::core::math3d::fx_div;
using saturn::core::math3d::fx_from_int;
using saturn::core::math3d::fx_mul;
using saturn::core::math3d::sin_deg_fx;
using saturn::core::math3d::vec3;
using saturn::core::math3d::vec3_cross_scaled;
using saturn::core::math3d::vec3_dot_raw;
using saturn::core::math3d::vec3_sub;

/* ------------------------------------------------------------------ */
/* Construction primitives                                             */
/* ------------------------------------------------------------------ */

inline sat_result_t init(
    sat_mesh_t* mesh,
    sat_vec3_t* vertices,
    uint16_t vertex_cap,
    uint16_t* indices,
    uint16_t face_cap
) {
    if (mesh == nullptr || vertices == nullptr || indices == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    mesh->vertices = vertices;
    mesh->indices = indices;
    mesh->vertex_cap = vertex_cap;
    mesh->vertex_count = 0u;
    mesh->face_cap = face_cap;
    mesh->face_count = 0u;
    return SAT_OK;
}

inline void clear(sat_mesh_t* mesh) {
    if (mesh == nullptr) {
        return;
    }
    mesh->vertex_count = 0u;
    mesh->face_count = 0u;
}

inline sat_result_t add_vertex(
    sat_mesh_t* mesh,
    sat_fx16_t x,
    sat_fx16_t y,
    sat_fx16_t z,
    uint16_t* out_index
) {
    if (mesh == nullptr || mesh->vertices == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (mesh->vertex_count >= mesh->vertex_cap) {
        return SAT_ERR_CAPACITY;
    }
    const uint16_t index = mesh->vertex_count;
    mesh->vertices[index] = vec3(x, y, z);
    mesh->vertex_count = static_cast<uint16_t>(index + 1u);
    if (out_index != nullptr) {
        *out_index = index;
    }
    return SAT_OK;
}

inline sat_result_t add_face(sat_mesh_t* mesh, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
    if (mesh == nullptr || mesh->indices == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (a >= mesh->vertex_count || b >= mesh->vertex_count ||
        c >= mesh->vertex_count || d >= mesh->vertex_count) {
        return SAT_ERR_INVALID_ARG;
    }
    if (mesh->face_count >= mesh->face_cap) {
        return SAT_ERR_CAPACITY;
    }
    uint16_t* slot = &mesh->indices[static_cast<uint32_t>(mesh->face_count) * 4u];
    slot[0] = a;
    slot[1] = b;
    slot[2] = c;
    slot[3] = d;
    mesh->face_count = static_cast<uint16_t>(mesh->face_count + 1u);
    return SAT_OK;
}

inline sat_result_t add_quad(sat_mesh_t* mesh, const sat_quad3_t* quad) {
    if (mesh == nullptr || quad == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (static_cast<uint32_t>(mesh->vertex_count) + 4u > mesh->vertex_cap ||
        mesh->face_count >= mesh->face_cap) {
        return SAT_ERR_CAPACITY;
    }
    uint16_t idx[4];
    for (int i = 0; i < 4; ++i) {
        const sat_result_t st =
            add_vertex(mesh, quad->v[i].x, quad->v[i].y, quad->v[i].z, &idx[i]);
        if (st != SAT_OK) {
            return st;
        }
    }
    return add_face(mesh, idx[0], idx[1], idx[2], idx[3]);
}

/* ------------------------------------------------------------------ */
/* Face queries                                                        */
/* ------------------------------------------------------------------ */

inline const uint16_t* face_indices(const sat_mesh_t* mesh, uint16_t face) {
    if (mesh == nullptr || mesh->indices == nullptr || face >= mesh->face_count) {
        return nullptr;
    }
    return &mesh->indices[static_cast<uint32_t>(face) * 4u];
}

inline sat_result_t face_quad(const sat_mesh_t* mesh, uint16_t face, sat_quad3_t* out) {
    const uint16_t* idx = face_indices(mesh, face);
    if (idx == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 4; ++i) {
        out->v[i] = mesh->vertices[idx[i]];
    }
    return SAT_OK;
}

inline sat_vec3_t quad_center(const sat_quad3_t& quad) {
    int64_t sx = 0;
    int64_t sy = 0;
    int64_t sz = 0;
    for (int i = 0; i < 4; ++i) {
        sx += quad.v[i].x;
        sy += quad.v[i].y;
        sz += quad.v[i].z;
    }
    return vec3(
        static_cast<sat_fx16_t>(sx / 4),
        static_cast<sat_fx16_t>(sy / 4),
        static_cast<sat_fx16_t>(sz / 4));
}

inline sat_result_t face_center(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out) {
    sat_quad3_t quad;
    if (out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_result_t st = face_quad(mesh, face, &quad);
    if (st != SAT_OK) {
        return st;
    }
    *out = quad_center(quad);
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

inline sat_result_t face_normal_scaled(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out) {
    sat_quad3_t quad;
    if (out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_result_t st = face_quad(mesh, face, &quad);
    if (st != SAT_OK) {
        return st;
    }
    *out = quad_normal_scaled(quad);
    return SAT_OK;
}

inline sat_result_t face_normal(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out) {
    const sat_result_t st = face_normal_scaled(mesh, face, out);
    if (st != SAT_OK) {
        return st;
    }
    *out = saturn::core::math3d::vec3_normalize(*out);
    return SAT_OK;
}

/* ------------------------------------------------------------------ */
/* Textured-face selection (host-testable, no VDP1 access)             */
/* ------------------------------------------------------------------ */

/* How one mesh face is drawn when a texture table may be bound. */
enum class mesh_face_draw_mode : uint8_t {
    kPolygon = 0,  /* flat-shaded polygon path */
    kTextured = 1, /* distorted-sprite path through textures[tex_index] */
    kInvalid = 2   /* face_texture_indices[face] is out of range */
};

inline mesh_face_draw_mode resolve_face_draw_mode(
    uint16_t face,
    const uint16_t* face_texture_indices,
    uint16_t face_count,
    const sat_vdp1_texture_t* textures,
    uint16_t texture_count,
    uint16_t* out_tex_index
) {
    if (out_tex_index != nullptr) {
        *out_tex_index = 0u;
    }
    if (face_texture_indices == nullptr) {
        return mesh_face_draw_mode::kPolygon;
    }
    if (face >= face_count) {
        return mesh_face_draw_mode::kInvalid;
    }
    const uint16_t sel = face_texture_indices[face];
    if (sel == SAT_MESH_TEXTURE_NONE) {
        return mesh_face_draw_mode::kPolygon;
    }
    if (textures == nullptr || sel >= texture_count) {
        return mesh_face_draw_mode::kInvalid;
    }
    if (out_tex_index != nullptr) {
        *out_tex_index = sel;
    }
    return mesh_face_draw_mode::kTextured;
}

/* Validates a whole face_texture_indices table without touching the VDP1.
 * Returns SAT_OK when every entry is SAT_MESH_TEXTURE_NONE or a valid index
 * into a table of texture_count entries, SAT_ERR_INVALID_ARG otherwise.
 * A null table is the legacy untextured mesh and is always valid. */
inline sat_result_t validate_face_textures(
    const uint16_t* face_texture_indices,
    uint16_t face_count,
    const sat_vdp1_texture_t* textures,
    uint16_t texture_count
) {
    if (face_texture_indices == nullptr) {
        return SAT_OK;
    }
    for (uint16_t i = 0; i < face_count; ++i) {
        const uint16_t sel = face_texture_indices[i];
        if (sel == SAT_MESH_TEXTURE_NONE) {
            continue;
        }
        if (textures == nullptr || sel >= texture_count) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    return SAT_OK;
}

/* A face is visible when its outward normal has a component towards the eye,
 * i.e. dot(normal, eye - center) > 0. The scaled normal is enough: scaling by
 * a positive factor cannot change the sign. */
inline bool quad_visible(const sat_quad3_t& quad, const sat_vec3_t& eye) {
    return vec3_dot_raw(quad_normal_scaled(quad), vec3_sub(eye, quad_center(quad))) > 0;
}

inline bool face_visible(const sat_mesh_t* mesh, uint16_t face, const sat_vec3_t& eye) {
    sat_quad3_t quad;
    if (face_quad(mesh, face, &quad) != SAT_OK) {
        return false;
    }
    return quad_visible(quad, eye);
}

/* Smooth vertex normals: each face's scaled normal added once to each of its
 * distinct corner vertices, then normalised. The scaled normals are shifted
 * down by 2 so even a vertex shared by thousands of faces cannot overflow. */
inline sat_result_t vertex_normals(const sat_mesh_t* mesh, sat_vec3_t* out, uint16_t cap) {
    if (mesh == nullptr || out == nullptr || mesh->vertices == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (cap < mesh->vertex_count) {
        return SAT_ERR_CAPACITY;
    }
    for (uint16_t v = 0; v < mesh->vertex_count; ++v) {
        out[v] = vec3(0, 0, 0);
    }
    for (uint16_t f = 0; f < mesh->face_count; ++f) {
        sat_quad3_t quad;
        if (face_quad(mesh, f, &quad) != SAT_OK) {
            continue;
        }
        const sat_vec3_t n = quad_normal_scaled(quad);
        const uint16_t* idx = face_indices(mesh, f);
        for (int i = 0; i < 4; ++i) {
            bool repeated = false;
            for (int j = 0; j < i; ++j) {
                repeated = repeated || idx[j] == idx[i];
            }
            if (repeated) {
                continue;
            }
            out[idx[i]].x += n.x >> 2;
            out[idx[i]].y += n.y >> 2;
            out[idx[i]].z += n.z >> 2;
        }
    }
    for (uint16_t v = 0; v < mesh->vertex_count; ++v) {
        out[v] = saturn::core::math3d::vec3_normalize(out[v]);
    }
    return SAT_OK;
}

/* Applies an affine matrix to every vertex, w assumed 1.
 *
 * Meant for orienting a primitive after building it. The builders all work in
 * one canonical pose -- a sphere's poles on Y, a cylinder's axis on Y -- so
 * anything that has to point somewhere else is built at the origin and then
 * moved, which is cheaper in code than a family of builders that each take an
 * axis. */
inline sat_result_t transform(sat_mesh_t* mesh, const sat_fx16_t* m) {
    if (mesh == nullptr || mesh->vertices == nullptr || m == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    for (uint16_t i = 0; i < mesh->vertex_count; ++i) {
        const sat_vec3_t v = mesh->vertices[i];
        const sat_vec4_t out =
            saturn::core::math3d::mat4_transform_vec4(m, v.x, v.y, v.z, SAT_FX16_ONE);
        mesh->vertices[i] = vec3(out.x, out.y, out.z);
    }
    return SAT_OK;
}

inline sat_result_t translate(sat_mesh_t* mesh, sat_fx16_t dx, sat_fx16_t dy, sat_fx16_t dz) {
    if (mesh == nullptr || mesh->vertices == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    for (uint16_t i = 0; i < mesh->vertex_count; ++i) {
        mesh->vertices[i].x += dx;
        mesh->vertices[i].y += dy;
        mesh->vertices[i].z += dz;
    }
    return SAT_OK;
}

/* O(n^2) in the surviving vertex count, which is what makes it cheap to call
 * once and wrong to call every frame: a few dozen vertices is a few hundred
 * compares, not a concern at startup, and exactly the cost sat_mesh_weld_vertices
 * exists to avoid paying every frame instead. */
inline sat_result_t weld_vertices(
    sat_mesh_t* mesh, sat_fx16_t epsilon, uint16_t* remap, uint16_t remap_cap
) {
    if (mesh == nullptr || mesh->vertices == nullptr || mesh->indices == nullptr ||
        remap == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (remap_cap < mesh->vertex_count) {
        return SAT_ERR_CAPACITY;
    }
    uint16_t kept = 0u;
    for (uint16_t v = 0; v < mesh->vertex_count; ++v) {
        const sat_vec3_t p = mesh->vertices[v];
        uint16_t k = 0u;
        for (; k < kept; ++k) {
            const sat_vec3_t q = mesh->vertices[k];
            if (sat_fx16_abs(p.x - q.x) < epsilon && sat_fx16_abs(p.y - q.y) < epsilon &&
                sat_fx16_abs(p.z - q.z) < epsilon) {
                break;
            }
        }
        if (k == kept) {
            mesh->vertices[kept++] = p;
        }
        remap[v] = k;
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(mesh->face_count) * 4u; ++i) {
        mesh->indices[i] = remap[mesh->indices[i]];
    }
    mesh->vertex_count = kept;
    return SAT_OK;
}

/* ------------------------------------------------------------------ */
/* Buffer sizing                                                       */
/* ------------------------------------------------------------------ */

inline uint16_t clamp_segments(uint16_t segments) {
    return (segments < 3u) ? 3u : segments;
}

inline uint16_t clamp_rings(uint16_t rings) {
    return (rings < 2u) ? 2u : rings;
}

inline void box_counts(uint16_t* v, uint16_t* f) {
    if (v != nullptr) { *v = 8u; }
    if (f != nullptr) { *f = 6u; }
}

inline void octahedron_counts(uint16_t* v, uint16_t* f) {
    if (v != nullptr) { *v = 6u; }
    if (f != nullptr) { *f = 8u; }
}

inline void plane_counts(uint16_t seg_x, uint16_t seg_z, uint16_t* v, uint16_t* f) {
    const uint32_t sx = (seg_x < 1u) ? 1u : seg_x;
    const uint32_t sz = (seg_z < 1u) ? 1u : seg_z;
    if (v != nullptr) { *v = static_cast<uint16_t>((sx + 1u) * (sz + 1u)); }
    if (f != nullptr) { *f = static_cast<uint16_t>(sx * sz); }
}

inline void sphere_counts(uint16_t segments, uint16_t rings, uint16_t* v, uint16_t* f) {
    const uint32_t s = clamp_segments(segments);
    const uint32_t r = clamp_rings(rings);
    if (v != nullptr) { *v = static_cast<uint16_t>((r + 1u) * s); }
    if (f != nullptr) { *f = static_cast<uint16_t>(r * s); }
}

inline void cylinder_counts(uint16_t segments, bool capped, uint16_t* v, uint16_t* f) {
    const uint32_t s = clamp_segments(segments);
    if (v != nullptr) { *v = static_cast<uint16_t>((2u * s) + (capped ? 2u : 0u)); }
    if (f != nullptr) { *f = static_cast<uint16_t>(s * (capped ? 3u : 1u)); }
}

/* ------------------------------------------------------------------ */
/* Builders                                                            */
/* ------------------------------------------------------------------ */

inline bool has_room(const sat_mesh_t* mesh, uint16_t vertices, uint16_t faces) {
    return mesh != nullptr && mesh->vertex_cap >= vertices && mesh->face_cap >= faces;
}

inline sat_result_t build_box(
    sat_mesh_t* mesh,
    const sat_vec3_t& center,
    sat_fx16_t hx,
    sat_fx16_t hy,
    sat_fx16_t hz
) {
    if (mesh == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (!has_room(mesh, 8u, 6u)) {
        return SAT_ERR_CAPACITY;
    }
    clear(mesh);
    /* Vertex index bits: 1 = +X, 2 = +Y, 4 = +Z. */
    for (int i = 0; i < 8; ++i) {
        add_vertex(
            mesh,
            center.x + (((i & 1) != 0) ? hx : -hx),
            center.y + (((i & 2) != 0) ? hy : -hy),
            center.z + (((i & 4) != 0) ? hz : -hz),
            nullptr);
    }
    add_face(mesh, 6u, 7u, 5u, 4u); /* +Z */
    add_face(mesh, 3u, 2u, 0u, 1u); /* -Z */
    add_face(mesh, 7u, 3u, 1u, 5u); /* +X */
    add_face(mesh, 2u, 6u, 4u, 0u); /* -X */
    add_face(mesh, 2u, 3u, 7u, 6u); /* +Y */
    add_face(mesh, 4u, 5u, 1u, 0u); /* -Y */
    return SAT_OK;
}

/* Eight outward-facing triangles over six shared vertices. A repeated D=C
 * lets the existing quad renderer emit each facet without fake padding
 * vertices. The alternating upper/lower face order also allows callers to
 * assign separate facet colors without constructing geometry themselves. */
inline sat_result_t build_octahedron(
    sat_mesh_t* mesh,
    const sat_vec3_t& center,
    sat_fx16_t radius,
    sat_fx16_t half_height
) {
    if (mesh == nullptr || mesh->vertices == nullptr || mesh->indices == nullptr ||
        radius <= 0 || half_height <= 0) {
        return SAT_ERR_INVALID_ARG;
    }
    if (!has_room(mesh, 6u, 8u)) {
        return SAT_ERR_CAPACITY;
    }
    clear(mesh);
    add_vertex(mesh, center.x + radius, center.y, center.z, nullptr); /* +X */
    add_vertex(mesh, center.x, center.y, center.z + radius, nullptr); /* +Z */
    add_vertex(mesh, center.x - radius, center.y, center.z, nullptr); /* -X */
    add_vertex(mesh, center.x, center.y, center.z - radius, nullptr); /* -Z */
    add_vertex(mesh, center.x, center.y + half_height, center.z, nullptr); /* top */
    add_vertex(mesh, center.x, center.y - half_height, center.z, nullptr); /* bottom */
    for (uint16_t side = 0u; side < 4u; ++side) {
        const uint16_t next = static_cast<uint16_t>((side + 1u) & 3u);
        add_face(mesh, 4u, side, next, next); /* upper outward triangle */
        add_face(mesh, side, 5u, next, next); /* lower outward triangle */
    }
    return SAT_OK;
}

inline sat_result_t build_plane(
    sat_mesh_t* mesh,
    const sat_vec3_t& center,
    sat_fx16_t half_x,
    sat_fx16_t half_z,
    uint16_t seg_x,
    uint16_t seg_z
) {
    uint16_t need_v = 0;
    uint16_t need_f = 0;
    const uint16_t sx = (seg_x < 1u) ? 1u : seg_x;
    const uint16_t sz = (seg_z < 1u) ? 1u : seg_z;
    plane_counts(sx, sz, &need_v, &need_f);
    if (mesh == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (!has_room(mesh, need_v, need_f)) {
        return SAT_ERR_CAPACITY;
    }
    clear(mesh);

    const sat_fx16_t x0 = center.x - half_x;
    const sat_fx16_t z0 = center.z - half_z;
    for (uint16_t j = 0; j <= sz; ++j) {
        const sat_fx16_t z =
            z0 + static_cast<sat_fx16_t>((static_cast<int64_t>(half_z) * 2 * j) / sz);
        for (uint16_t i = 0; i <= sx; ++i) {
            const sat_fx16_t x =
                x0 + static_cast<sat_fx16_t>((static_cast<int64_t>(half_x) * 2 * i) / sx);
            add_vertex(mesh, x, center.y, z, nullptr);
        }
    }
    const uint16_t stride = static_cast<uint16_t>(sx + 1u);
    for (uint16_t j = 0; j < sz; ++j) {
        for (uint16_t i = 0; i < sx; ++i) {
            const uint16_t a = static_cast<uint16_t>((j * stride) + i);
            add_face(
                mesh,
                a,
                static_cast<uint16_t>(a + 1u),
                static_cast<uint16_t>(a + stride + 1u),
                static_cast<uint16_t>(a + stride));
        }
    }
    return SAT_OK;
}

inline sat_result_t build_sphere(
    sat_mesh_t* mesh,
    const sat_vec3_t& center,
    sat_fx16_t radius,
    uint16_t segments,
    uint16_t rings
) {
    const uint16_t s = clamp_segments(segments);
    const uint16_t r = clamp_rings(rings);
    uint16_t need_v = 0;
    uint16_t need_f = 0;
    sphere_counts(s, r, &need_v, &need_f);
    if (mesh == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (!has_room(mesh, need_v, need_f)) {
        return SAT_ERR_CAPACITY;
    }
    clear(mesh);

    /* Latitude 0 is the +Y pole. Longitude 0 points along +Z, and increasing
     * longitude swings towards +X, which is the direction that makes the
     * A -> B corner order run left-to-right when the face is seen from
     * outside. */
    for (uint16_t i = 0; i <= r; ++i) {
        const sat_fx16_t theta = fx_div(fx_from_int(180 * i), fx_from_int(r));
        const sat_fx16_t y = fx_mul(radius, cos_deg_fx(theta));
        const sat_fx16_t ring_r = fx_mul(radius, sin_deg_fx(theta));
        for (uint16_t j = 0; j < s; ++j) {
            const sat_fx16_t phi = fx_div(fx_from_int(360 * j), fx_from_int(s));
            add_vertex(
                mesh,
                center.x + fx_mul(ring_r, sin_deg_fx(phi)),
                center.y + y,
                center.z + fx_mul(ring_r, cos_deg_fx(phi)),
                nullptr);
        }
    }
    for (uint16_t i = 0; i < r; ++i) {
        for (uint16_t j = 0; j < s; ++j) {
            const uint16_t j1 = static_cast<uint16_t>((j + 1u) % s);
            const uint16_t top = static_cast<uint16_t>(i * s);
            const uint16_t bottom = static_cast<uint16_t>((i + 1u) * s);
            add_face(
                mesh,
                static_cast<uint16_t>(top + j),
                static_cast<uint16_t>(top + j1),
                static_cast<uint16_t>(bottom + j1),
                static_cast<uint16_t>(bottom + j));
        }
    }
    return SAT_OK;
}

/* Sphere with an angular sector removed, closed by flat faces.
 *
 * The removed sector is a lune between two meridians, so from above the solid
 * reads as a pie chart with a slice taken out -- which is what a Pac-Man is.
 * Cutting the geometry is not the same as drawing a dark wedge over a whole
 * sphere: the silhouette changes, the cut walls catch the light differently
 * from the outside of the sphere, and the shape stays right from any angle.
 *
 * Longitude 0 points along +Z and increases towards +X, matching
 * build_sphere. A gap of zero segments gives a plain sphere with two unused
 * axis vertices, which is why build_sphere stays a separate function. */
inline void sphere_wedge_counts(
    uint16_t segments,
    uint16_t rings,
    uint16_t gap_segments,
    uint16_t* v,
    uint16_t* f
) {
    const uint32_t s = clamp_segments(segments);
    const uint32_t r = clamp_rings(rings);
    /* At least one band has to survive, or there is no solid left. */
    const uint32_t gap = (gap_segments >= s) ? (s - 1u) : gap_segments;
    if (v != nullptr) {
        *v = static_cast<uint16_t>(((r + 1u) * s) + (r + 1u));
    }
    if (f != nullptr) {
        /* The surface, minus the removed bands, plus two flat cut walls. */
        *f = static_cast<uint16_t>((r * (s - gap)) + ((gap > 0u) ? (2u * r) : 0u));
    }
}

inline sat_result_t build_sphere_wedge(
    sat_mesh_t* mesh,
    const sat_vec3_t& center,
    sat_fx16_t radius,
    uint16_t segments,
    uint16_t rings,
    uint16_t gap_start_segment,
    uint16_t gap_segments
) {
    const uint16_t s = clamp_segments(segments);
    const uint16_t r = clamp_rings(rings);
    const uint16_t gap = (gap_segments >= s) ? static_cast<uint16_t>(s - 1u) : gap_segments;
    const uint16_t gap_start = static_cast<uint16_t>(gap_start_segment % s);
    uint16_t need_v = 0;
    uint16_t need_f = 0;
    uint16_t i;
    uint16_t j;

    sphere_wedge_counts(s, r, gap, &need_v, &need_f);
    if (mesh == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (!has_room(mesh, need_v, need_f)) {
        return SAT_ERR_CAPACITY;
    }
    clear(mesh);

    for (i = 0; i <= r; ++i) {
        const sat_fx16_t theta = fx_div(fx_from_int(180 * i), fx_from_int(r));
        const sat_fx16_t y = fx_mul(radius, cos_deg_fx(theta));
        const sat_fx16_t ring_r = fx_mul(radius, sin_deg_fx(theta));
        for (j = 0; j < s; ++j) {
            const sat_fx16_t phi = fx_div(fx_from_int(360 * j), fx_from_int(s));
            add_vertex(
                mesh,
                center.x + fx_mul(ring_r, sin_deg_fx(phi)),
                center.y + y,
                center.z + fx_mul(ring_r, cos_deg_fx(phi)),
                nullptr);
        }
    }
    /* The axis, one point per latitude: the inner edge of both cut walls. */
    const uint16_t axis = static_cast<uint16_t>((r + 1u) * s);
    for (i = 0; i <= r; ++i) {
        const sat_fx16_t theta = fx_div(fx_from_int(180 * i), fx_from_int(r));
        add_vertex(mesh, center.x, center.y + fx_mul(radius, cos_deg_fx(theta)), center.z, nullptr);
    }

    for (i = 0; i < r; ++i) {
        const uint16_t top = static_cast<uint16_t>(i * s);
        const uint16_t bottom = static_cast<uint16_t>((i + 1u) * s);
        for (j = 0; j < s; ++j) {
            /* Band j spans longitudes j..j+1, and is removed when it falls in
             * the gap. The comparison is done on a rotated index so a gap
             * that wraps past longitude 0 needs no special case. */
            const uint16_t rotated = static_cast<uint16_t>((j + s - gap_start) % s);
            const uint16_t j1 = static_cast<uint16_t>((j + 1u) % s);
            if (rotated < gap) {
                continue;
            }
            add_face(
                mesh,
                static_cast<uint16_t>(top + j),
                static_cast<uint16_t>(top + j1),
                static_cast<uint16_t>(bottom + j1),
                static_cast<uint16_t>(bottom + j));
        }
    }

    if (gap == 0u) {
        return SAT_OK;
    }

    /* The two cut walls, each a fan from the axis out to the rim.
     *
     * Their normals face INTO the gap -- outwards as far as the remaining
     * solid is concerned -- so the winding is mirrored between the two: the
     * solid lies at lower longitude than the start meridian and at higher
     * longitude than the end one. */
    const uint16_t end_j = static_cast<uint16_t>((gap_start + gap) % s);
    for (i = 0; i < r; ++i) {
        const uint16_t top = static_cast<uint16_t>(i * s);
        const uint16_t bottom = static_cast<uint16_t>((i + 1u) * s);
        add_face(
            mesh,
            static_cast<uint16_t>(axis + i + 1u),
            static_cast<uint16_t>(bottom + gap_start),
            static_cast<uint16_t>(top + gap_start),
            static_cast<uint16_t>(axis + i));
        add_face(
            mesh,
            static_cast<uint16_t>(axis + i),
            static_cast<uint16_t>(top + end_j),
            static_cast<uint16_t>(bottom + end_j),
            static_cast<uint16_t>(axis + i + 1u));
    }
    return SAT_OK;
}

inline sat_result_t build_cylinder(
    sat_mesh_t* mesh,
    const sat_vec3_t& center,
    sat_fx16_t radius,
    sat_fx16_t half_height,
    uint16_t segments,
    bool capped
) {
    const uint16_t s = clamp_segments(segments);
    uint16_t need_v = 0;
    uint16_t need_f = 0;
    cylinder_counts(s, capped, &need_v, &need_f);
    if (mesh == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (!has_room(mesh, need_v, need_f)) {
        return SAT_ERR_CAPACITY;
    }
    clear(mesh);

    for (uint16_t j = 0; j < s; ++j) {
        const sat_fx16_t phi = fx_div(fx_from_int(360 * j), fx_from_int(s));
        const sat_fx16_t x = center.x + fx_mul(radius, sin_deg_fx(phi));
        const sat_fx16_t z = center.z + fx_mul(radius, cos_deg_fx(phi));
        add_vertex(mesh, x, center.y + half_height, z, nullptr);
    }
    for (uint16_t j = 0; j < s; ++j) {
        const sat_fx16_t phi = fx_div(fx_from_int(360 * j), fx_from_int(s));
        const sat_fx16_t x = center.x + fx_mul(radius, sin_deg_fx(phi));
        const sat_fx16_t z = center.z + fx_mul(radius, cos_deg_fx(phi));
        add_vertex(mesh, x, center.y - half_height, z, nullptr);
    }
    uint16_t top_hub = 0;
    uint16_t bottom_hub = 0;
    if (capped) {
        add_vertex(mesh, center.x, center.y + half_height, center.z, &top_hub);
        add_vertex(mesh, center.x, center.y - half_height, center.z, &bottom_hub);
    }

    for (uint16_t j = 0; j < s; ++j) {
        const uint16_t j1 = static_cast<uint16_t>((j + 1u) % s);
        add_face(
            mesh,
            j,
            j1,
            static_cast<uint16_t>(s + j1),
            static_cast<uint16_t>(s + j));
    }
    if (capped) {
        /* Fans built as quads with the hub repeated, so the mesh stays
         * quad-only and every face still submits as one VDP1 command. */
        for (uint16_t j = 0; j < s; ++j) {
            const uint16_t j1 = static_cast<uint16_t>((j + 1u) % s);
            add_face(mesh, top_hub, j1, j, top_hub);
        }
        for (uint16_t j = 0; j < s; ++j) {
            const uint16_t j1 = static_cast<uint16_t>((j + 1u) % s);
            add_face(
                mesh,
                bottom_hub,
                static_cast<uint16_t>(s + j),
                static_cast<uint16_t>(s + j1),
                bottom_hub);
        }
    }
    return SAT_OK;
}

}  // namespace saturn::core::mesh3d

#endif /* SATURN_CORE_MESH3D_LOGIC_HPP */
