#include "saturn/mesh3d.h"

#include "src/graphics/3d/geometry/mesh_logic.hpp"
#include "src/graphics/3d/rendering/logic.hpp"

using namespace saturn::core::mesh3d;

namespace {

/* Painter's key: squared 3D eye distance at 8 fractional bits.
 *
 * The public ground_distance_sq keys on whole units in the XZ plane, which is
 * right for maze-scale scenes but collapses every face of a sub-unit model
 * (an animated character spans ~0.5 units) onto the same key, so the "sorted"
 * draw came out in index order. Eight fractional bits resolve 1/256 unit and
 * still fit ~147 units per axis in 32 bits; farther faces clamp together,
 * where their order no longer matters. */
inline uint32_t eye_distance_key(const sat_vec3_t& eye, const sat_vec3_t& c) {
    const int64_t dx = (static_cast<int64_t>(eye.x) - c.x) >> 8;
    const int64_t dy = (static_cast<int64_t>(eye.y) - c.y) >> 8;
    const int64_t dz = (static_cast<int64_t>(eye.z) - c.z) >> 8;
    const int64_t d = dx * dx + dy * dy + dz * dz;
    return d > static_cast<int64_t>(0xFFFFFFFFu) ? 0xFFFFFFFFu : static_cast<uint32_t>(d);
}

/* ------------------------------------------------------------------ */
/* Screen-space path (sat_mesh_draw_t::screen)                          */
/* ------------------------------------------------------------------ */

inline void fill_quad2(const sat_projected_vertex_t* screen, const uint16_t* idx, sat_quad2_t* out) {
    for (int i = 0; i < 4; ++i) {
        out->x[i] = screen[idx[i]].x;
        out->y[i] = screen[idx[i]].y;
    }
}

/* Painter's key from view depth: the corners' clip w, a quarter each so the
 * sum of four positive 16.16 values stays inside 32 bits. */
inline uint32_t view_depth_key(const sat_projected_vertex_t* screen, const uint16_t* idx) {
    uint32_t key = 0u;
    for (int i = 0; i < 4; ++i) {
        key += static_cast<uint32_t>(screen[idx[i]].w) >> 2u;
    }
    return key;
}

sat_result_t submit_projected_face(
    const sat_mesh_t* mesh,
    const sat_mesh_draw_t* params,
    uint16_t face,
    const sat_quad2_t& projected
) {
    uint16_t tex_index = 0u;
    const mesh_face_draw_mode mode = resolve_face_draw_mode(
        face,
        params->face_texture_indices,
        mesh->face_count,
        params->textures,
        params->texture_count,
        &tex_index);
    if (mode == mesh_face_draw_mode::kTextured) {
        return sat_draw_quad2_sprite(&projected, &params->textures[tex_index], 0u, 0u);
    }
    uint16_t color = (params->face_colors != nullptr) ? params->face_colors[face] : params->color;
    if ((params->flags & SAT_MESH_SHADE) != 0u) {
        sat_quad3_t quad;
        if (face_quad(mesh, face, &quad) == SAT_OK) {
            color = saturn::core::render3d::shade_rgb555(
                color,
                saturn::core::render3d::face_intensity3_scaled(
                    quad_normal_scaled(quad), params->ambient));
        }
    }
    if (params->vertex_gouraud != nullptr) {
        const uint16_t* idx = &mesh->indices[static_cast<uint32_t>(face) * 4u];
        const uint16_t table[4] = {
            params->vertex_gouraud[idx[0]], params->vertex_gouraud[idx[1]],
            params->vertex_gouraud[idx[2]], params->vertex_gouraud[idx[3]],
        };
        return sat_draw_quad2_polygon_gouraud(&projected, color, table);
    }
    return sat_draw_quad2_polygon(&projected, color);
}

/* Projects every vertex once, then culls, sorts and submits from screen
 * space. Per face that is a handful of int16 products and additions, where
 * the world-space path pays a 3D cross product, a center and a 64-bit
 * distance for every face before it can even decide to skip it. */
sat_result_t draw_mesh_projected(
    const sat_mesh_t* mesh,
    const sat_mesh_draw_t* params,
    bool sorted,
    bool wide
) {
    const sat_projected_vertex_t* screen = params->screen;
    sat_result_t st = sat_project_vertices(
        params->view_proj, mesh->vertices, mesh->vertex_count, params->screen);
    if (st != SAT_OK) {
        return st;
    }
    const bool cull = (params->flags & SAT_MESH_CULL_BACKFACE) != 0u;
    /* One pass over the vertices decides whether any face can straddle the
     * camera plane; when none can -- the usual case for a framed model --
     * the per-face loop skips four depth loads per face. */
    bool any_behind = false;
    for (uint16_t v = 0; v < mesh->vertex_count; ++v) {
        if (screen[v].w <= 0) {
            any_behind = true;
            break;
        }
    }
    sat_result_t worst = SAT_OK;
    for (uint16_t f = 0; f < mesh->face_count; ++f) {
        const uint16_t* idx = &mesh->indices[static_cast<uint32_t>(f) * 4u];
        /* The VDP1 cannot clip at the near plane: a straddling face is not
         * drawable, which is a visibility outcome, not an error. */
        bool drawable = !any_behind ||
            (screen[idx[0]].w > 0 && screen[idx[1]].w > 0 &&
             screen[idx[2]].w > 0 && screen[idx[3]].w > 0);
        if (drawable && cull) {
            drawable = saturn::core::render3d::projected_area2(screen, idx) > 0;
        }
        if (!sorted) {
            if (drawable) {
                sat_quad2_t projected;
                fill_quad2(screen, idx, &projected);
                st = submit_projected_face(mesh, params, f, projected);
                if (st != SAT_OK && st != SAT_ERR_UNSUPPORTED) {
                    worst = st;
                }
            }
            continue;
        }
        params->depth[f] = drawable ? view_depth_key(screen, idx) : saturn::core::render3d::kPaintSkip;
    }
    if (!sorted) {
        return worst;
    }
    const uint32_t live = wide
        ? saturn::core::render3d::paint_order_buckets(params->depth, mesh->face_count, params->order16)
        : saturn::core::render3d::paint_order_buckets(params->depth, mesh->face_count, params->order);
    for (uint32_t n = 0; n < live; ++n) {
        const uint16_t f = wide ? params->order16[n] : params->order[n];
        sat_quad2_t projected;
        fill_quad2(screen, &mesh->indices[static_cast<uint32_t>(f) * 4u], &projected);
        st = submit_projected_face(mesh, params, f, projected);
        if (st != SAT_OK && st != SAT_ERR_UNSUPPORTED) {
            worst = st;
        }
    }
    return worst;
}

}  // namespace

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

extern "C" sat_result_t sat_vdp1_draw_mesh(const sat_mesh_t* mesh, const sat_mesh_draw_t* params) {
    if (mesh == nullptr || params == nullptr || params->view_proj == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const bool sorted = (params->flags & SAT_MESH_SORT) != 0u;
    /* Meshes past the legacy 255-face uint8_t limit sort through order16;
     * smaller meshes keep the legacy order table untouched. */
    const bool wide = mesh->face_count > 255u;
    if (sorted && wide &&
        (params->order16 == nullptr || params->depth == nullptr)) {
        return SAT_ERR_INVALID_ARG;
    }
    if (sorted && !wide &&
        (params->order == nullptr || params->depth == nullptr)) {
        return SAT_ERR_INVALID_ARG;
    }
    /* A face texture index must never read past the texture table. Validate
     * the whole table up front so the error does not depend on which faces
     * happen to survive culling from this camera position. */
    if (validate_face_textures(
            params->face_texture_indices,
            mesh->face_count,
            params->textures,
            params->texture_count) != SAT_OK) {
        return SAT_ERR_INVALID_ARG;
    }
    if (params->screen != nullptr) {
        return draw_mesh_projected(mesh, params, sorted, wide);
    }

    /* Building the draw order first, rather than culling inside the submit
     * loop, keeps the sort keyed on the faces that actually survive -- and
     * means a mesh drawn without SAT_MESH_SORT costs no scratch at all. */
    using saturn::core::render3d::kPaintSkip;
    uint32_t total = mesh->face_count;
    if (sorted) {
        for (uint16_t i = 0; i < mesh->face_count; ++i) {
            sat_quad3_t quad;
            if (face_quad(mesh, i, &quad) != SAT_OK ||
                ((params->flags & SAT_MESH_CULL_BACKFACE) != 0u &&
                 !quad_visible(quad, params->eye))) {
                params->depth[i] = kPaintSkip;
                continue;
            }
            const uint32_t key = eye_distance_key(params->eye, quad_center(quad));
            params->depth[i] = (key == kPaintSkip) ? kPaintSkip - 1u : key;
        }
        total = wide
            ? saturn::core::render3d::paint_order_buckets(params->depth, mesh->face_count, params->order16)
            : saturn::core::render3d::paint_order_buckets(params->depth, mesh->face_count, params->order);
    }

    sat_result_t worst = SAT_OK;
    for (uint32_t n = 0; n < total; ++n) {
        const uint16_t face = sorted ? (wide ? params->order16[n] : params->order[n]) : static_cast<uint16_t>(n);
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
        /* Shared culling/sorting above; only the submit path forks here. */
        uint16_t tex_index = 0u;
        const mesh_face_draw_mode mode = resolve_face_draw_mode(
            face,
            params->face_texture_indices,
            mesh->face_count,
            params->textures,
            params->texture_count,
            &tex_index);
        sat_result_t st = SAT_OK;
        const bool textured = mode == mesh_face_draw_mode::kTextured;
        /* Textured faces bypass SAT_MESH_SHADE: the VDP1 distorted-sprite
         * path has no per-face RGB modulation matching the polygon path. */
        if (!textured && (params->flags & SAT_MESH_SHADE) != 0u) {
            color = saturn::core::render3d::shade_rgb555(
                color,
                saturn::core::render3d::face_intensity3_scaled(
                    quad_normal_scaled(quad), params->ambient));
        }
        if (textured) {
            st = sat_draw_world_sprite(
                params->view_proj, &quad, &params->textures[tex_index], 0u, 0u);
        } else if (params->vertex_gouraud != nullptr) {
            const uint16_t* idx = face_indices(mesh, face);
            const uint16_t table[4] = {
                params->vertex_gouraud[idx[0]], params->vertex_gouraud[idx[1]],
                params->vertex_gouraud[idx[2]], params->vertex_gouraud[idx[3]],
            };
            st = sat_draw_world_polygon_gouraud(params->view_proj, &quad, color, table);
        } else {
            st = sat_draw_world_polygon(params->view_proj, &quad, color);
        }
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
