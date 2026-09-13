#include "saturn/mesh3d.h"

#include "src/core/mesh3d_logic.hpp"
#include "src/core/render3d_logic.hpp"

using namespace saturn::core::mesh3d;

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

extern "C" int sat_mesh_face_visible(const sat_mesh_t* mesh, uint16_t face, const sat_vec3_t* eye) {
    if (eye == nullptr) {
        return 0;
    }
    return face_visible(mesh, face, *eye) ? 1 : 0;
}

extern "C" sat_result_t sat_mesh_translate(sat_mesh_t* mesh, sat_fx16_t dx, sat_fx16_t dy, sat_fx16_t dz) {
    return translate(mesh, dx, dy, dz);
}

extern "C" sat_result_t sat_mesh_transform(sat_mesh_t* mesh, const sat_mat4_t* matrix) {
    if (matrix == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return transform(mesh, matrix->m);
}

extern "C" sat_result_t sat_draw_mesh(const sat_mesh_t* mesh, const sat_mesh_draw_t* params) {
    if (mesh == nullptr || params == nullptr || params->view_proj == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const bool sorted = (params->flags & SAT_MESH_SORT) != 0u;
    if (sorted && (params->order == nullptr || params->depth == nullptr || mesh->face_count > 255u)) {
        return SAT_ERR_INVALID_ARG;
    }

    /* Building the draw order first, rather than culling inside the submit
     * loop, keeps the sort keyed on the faces that actually survive -- and
     * means a mesh drawn without SAT_MESH_SORT costs no scratch at all. */
    uint16_t count = mesh->face_count;
    if (sorted) {
        uint8_t live = 0u;
        for (uint16_t i = 0; i < mesh->face_count; ++i) {
            sat_quad3_t quad;
            if (face_quad(mesh, i, &quad) != SAT_OK) {
                continue;
            }
            if ((params->flags & SAT_MESH_CULL_BACKFACE) != 0u &&
                !quad_visible(quad, params->eye)) {
                continue;
            }
            const sat_vec3_t center = quad_center(quad);
            params->order[live] = static_cast<uint8_t>(i);
            params->depth[i] = saturn::core::render3d::ground_distance_sq(
                params->eye.x, params->eye.z, center.x, center.z);
            ++live;
        }
        count = live;
        saturn::core::render3d::sort_indices_desc(params->order, params->depth, count);
    }

    sat_result_t worst = SAT_OK;
    for (uint16_t n = 0; n < count; ++n) {
        const uint16_t face = sorted ? params->order[n] : n;
        sat_quad3_t quad;
        uint16_t color =
            (params->face_colors != nullptr) ? params->face_colors[face] : params->color;

        if (face_quad(mesh, face, &quad) != SAT_OK) {
            continue;
        }
        /* Already culled above when sorting; only the unsorted path still
         * has to test, and it tests from the quad it just fetched rather
         * than fetching it a second time. */
        if (!sorted && (params->flags & SAT_MESH_CULL_BACKFACE) != 0u &&
            !quad_visible(quad, params->eye)) {
            continue;
        }
        if ((params->flags & SAT_MESH_SHADE) != 0u) {
            color = saturn::core::render3d::shade_rgb555(
                color,
                saturn::core::render3d::face_intensity3_scaled(
                    quad_normal_scaled(quad), params->ambient));
        }

        const sat_result_t st = sat_draw_world_polygon(params->view_proj, &quad, color);
        /* A quad crossing the near plane cannot be drawn on hardware with no
         * clipper; that is a visibility outcome, not a failure. Running out of
         * VDP1 commands is a real one, and is reported after the loop so the
         * rest of the mesh still gets its chance. */
        if (st != SAT_OK && st != SAT_ERR_UNSUPPORTED) {
            worst = st;
        }
    }
    return worst;
}
