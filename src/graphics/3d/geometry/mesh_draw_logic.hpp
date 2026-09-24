#ifndef SATURN_CORE_MESH3D_DRAW_LOGIC_HPP
#define SATURN_CORE_MESH3D_DRAW_LOGIC_HPP

#include "saturn/mesh3d_draw.h"
#include "src/graphics/3d/geometry/mesh_logic.hpp"

namespace saturn::core::mesh3d {

/* Optional material-selection policy belongs beside VDP1 mesh submission.
 * The geometric mesh builders and collision helpers never include VDP1. */
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


} // namespace saturn::core::mesh3d

#endif /* SATURN_CORE_MESH3D_DRAW_LOGIC_HPP */
