#include "saturn/mesh3d.h"
#include "src/core/geometry/mesh_logic.hpp"

using namespace saturn::core::mesh3d;

/* Standalone mesh C API: no VDP1 symbols, scene state or hardware init. */

extern "C" sat_result_t sat_mesh_init(
    sat_mesh_t* mesh,
    sat_vec3_t* vertices,
    uint16_t vertex_cap,
    uint16_t* indices,
    uint16_t face_cap
) {
    return init(mesh, vertices, vertex_cap, indices, face_cap);
}

extern "C" void sat_mesh_clear(sat_mesh_t* mesh) {
    clear(mesh);
}

extern "C" sat_result_t sat_mesh_add_vertex(
    sat_mesh_t* mesh,
    sat_fx16_t x,
    sat_fx16_t y,
    sat_fx16_t z,
    uint16_t* out_index
) {
    return add_vertex(mesh, x, y, z, out_index);
}

extern "C" sat_result_t sat_mesh_add_face(sat_mesh_t* mesh, uint16_t a, uint16_t b, uint16_t c, uint16_t d) {
    return add_face(mesh, a, b, c, d);
}

extern "C" sat_result_t sat_mesh_add_quad(sat_mesh_t* mesh, const sat_quad3_t* quad) {
    return add_quad(mesh, quad);
}

extern "C" void sat_mesh_box_counts(uint16_t* out_vertices, uint16_t* out_faces) {
    box_counts(out_vertices, out_faces);
}

extern "C" void sat_mesh_octahedron_counts(uint16_t* out_vertices, uint16_t* out_faces) {
    octahedron_counts(out_vertices, out_faces);
}

extern "C" void sat_mesh_plane_counts(
    uint16_t seg_x,
    uint16_t seg_z,
    uint16_t* out_vertices,
    uint16_t* out_faces
) {
    plane_counts(seg_x, seg_z, out_vertices, out_faces);
}

extern "C" void sat_mesh_sphere_counts(
    uint16_t segments,
    uint16_t rings,
    uint16_t* out_vertices,
    uint16_t* out_faces
) {
    sphere_counts(segments, rings, out_vertices, out_faces);
}

extern "C" void sat_mesh_cylinder_counts(
    uint16_t segments,
    int capped,
    uint16_t* out_vertices,
    uint16_t* out_faces
) {
    cylinder_counts(segments, capped != 0, out_vertices, out_faces);
}

extern "C" sat_result_t sat_mesh_build_box(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t half_x,
    sat_fx16_t half_y,
    sat_fx16_t half_z
) {
    if (center == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return build_box(mesh, *center, half_x, half_y, half_z);
}

extern "C" sat_result_t sat_mesh_build_octahedron(
    sat_mesh_t* mesh, const sat_vec3_t* center,
    sat_fx16_t radius, sat_fx16_t half_height
) {
    if (center == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return build_octahedron(mesh, *center, radius, half_height);
}

extern "C" sat_result_t sat_mesh_build_cube(sat_mesh_t* mesh, const sat_vec3_t* center, sat_fx16_t half) {
    if (center == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return build_box(mesh, *center, half, half, half);
}

extern "C" sat_result_t sat_mesh_build_plane(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t half_x,
    sat_fx16_t half_z,
    uint16_t seg_x,
    uint16_t seg_z
) {
    if (center == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return build_plane(mesh, *center, half_x, half_z, seg_x, seg_z);
}

extern "C" sat_result_t sat_mesh_build_sphere(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t radius,
    uint16_t segments,
    uint16_t rings
) {
    if (center == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return build_sphere(mesh, *center, radius, segments, rings);
}

extern "C" void sat_mesh_sphere_wedge_counts(
    uint16_t segments,
    uint16_t rings,
    uint16_t gap_segments,
    uint16_t* out_vertices,
    uint16_t* out_faces
) {
    sphere_wedge_counts(segments, rings, gap_segments, out_vertices, out_faces);
}

extern "C" sat_result_t sat_mesh_build_sphere_wedge(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t radius,
    uint16_t segments,
    uint16_t rings,
    uint16_t gap_start_segment,
    uint16_t gap_segments
) {
    if (center == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return build_sphere_wedge(
        mesh, *center, radius, segments, rings, gap_start_segment, gap_segments);
}

extern "C" sat_result_t sat_mesh_build_cylinder(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t radius,
    sat_fx16_t half_height,
    uint16_t segments,
    int capped
) {
    if (center == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return build_cylinder(mesh, *center, radius, half_height, segments, capped != 0);
}

extern "C" sat_result_t sat_mesh_face_quad(const sat_mesh_t* mesh, uint16_t face, sat_quad3_t* out) {
    return face_quad(mesh, face, out);
}

extern "C" sat_result_t sat_mesh_face_center(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out) {
    return face_center(mesh, face, out);
}

extern "C" sat_result_t sat_mesh_face_normal(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out) {
    return face_normal(mesh, face, out);
}

extern "C" sat_result_t sat_mesh_face_normal_scaled(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out) {
    return face_normal_scaled(mesh, face, out);
}

extern "C" sat_result_t sat_mesh_vertex_normals(
    const sat_mesh_t* mesh,
    sat_vec3_t* out_normals,
    uint16_t normal_cap
) {
    return vertex_normals(mesh, out_normals, normal_cap);
}

extern "C" int sat_mesh_face_visible(const sat_mesh_t* mesh, uint16_t face, const sat_vec3_t* eye) {
    if (eye == nullptr) {
        return 0;
    }
    return face_visible(mesh, face, *eye) ? 1 : 0;
}

extern "C" sat_result_t sat_mesh_translate(sat_mesh_t* mesh, sat_fx16_t dx, sat_fx16_t dy, sat_fx16_t dz) {
    return translate(mesh, dx, dy, dz);
}

extern "C" sat_result_t sat_mesh_weld_vertices(
    sat_mesh_t* mesh, sat_fx16_t epsilon, uint16_t* remap_scratch, uint16_t remap_cap
) {
    return weld_vertices(mesh, epsilon, remap_scratch, remap_cap);
}

extern "C" sat_result_t sat_mesh_transform(sat_mesh_t* mesh, const sat_mat4_t* matrix) {
    if (matrix == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return transform(mesh, matrix->m);
}

