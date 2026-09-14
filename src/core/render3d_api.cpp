#include "saturn/render3d.h"

#include "src/core/render3d_logic.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/vdp1.hpp"

using namespace saturn::core::render3d;

namespace {

/* Projection needs the active resolution, so these entry points require an
 * initialized runtime rather than taking screen dimensions from the caller --
 * the caller already told sat_init what they are. */
sat_result_t project(const sat_mat4_t* view_proj, const sat_quad3_t* quad, sat_quad2_t* out) {
    const sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (view_proj == nullptr || quad == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_video_config_t& cfg = saturn::core::g_state.config;
    if (!project_quad(
            view_proj->m,
            quad,
            static_cast<int16_t>(cfg.width),
            static_cast<int16_t>(cfg.height),
            out)) {
        return SAT_ERR_UNSUPPORTED;
    }
    return SAT_OK;
}

}  // namespace

extern "C" void sat_quad3_wall(
    sat_quad3_t* out,
    sat_fx16_t x0,
    sat_fx16_t z0,
    sat_fx16_t x1,
    sat_fx16_t z1,
    sat_fx16_t height
) {
    make_wall(out, x0, z0, x1, z1, height);
}

extern "C" void sat_quad3_floor(
    sat_quad3_t* out,
    sat_fx16_t cx,
    sat_fx16_t y,
    sat_fx16_t cz,
    sat_fx16_t half
) {
    make_floor(out, cx, y, cz, half);
}

extern "C" void sat_quad3_billboard(
    sat_quad3_t* out,
    sat_fx16_t cx,
    sat_fx16_t cz,
    sat_fx16_t right_x,
    sat_fx16_t right_z,
    sat_fx16_t half_w,
    sat_fx16_t height
) {
    make_billboard(out, cx, cz, right_x, right_z, half_w, height);
}

extern "C" sat_result_t sat_project_quad(
    const sat_mat4_t* view_proj,
    const sat_quad3_t* quad,
    sat_quad2_t* out
) {
    return project(view_proj, quad, out);
}

extern "C" sat_result_t sat_draw_world_polygon(
    const sat_mat4_t* view_proj,
    const sat_quad3_t* quad,
    uint16_t color
) {
    sat_quad2_t projected = {};
    const sat_result_t st = project(view_proj, quad, &projected);
    if (st != SAT_OK) {
        return st;
    }

    sat_polygon_cmd_t cmd = {};
    for (int i = 0; i < 4; ++i) {
        cmd.x[i] = projected.x[i];
        cmd.y[i] = projected.y[i];
    }
    cmd.color = color;
    cmd.flags = 0;
    return sat_draw_polygon(&cmd);
}

/* Goes straight to the HAL rather than through sat_draw_polygon.
 *
 * That is not premature: replaying a baked scene submits several hundred of
 * these per frame, and each extra layer copies the same eight coordinates
 * into another struct. Profiling pacman_3d put the whole submission path at
 * about 40% of its frame, spread evenly across the layers. */
extern "C" sat_result_t sat_draw_quad2_polygon(const sat_quad2_t* quad, uint16_t color) {
    const sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (quad == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    saturn::hal::vdp1::PolygonRequest req = {};
    req.xa = quad->x[0];
    req.ya = quad->y[0];
    req.xb = quad->x[1];
    req.yb = quad->y[1];
    req.xc = quad->x[2];
    req.yc = quad->y[2];
    req.xd = quad->x[3];
    req.yd = quad->y[3];
    req.color = color;
    req.flags = 0;
    return saturn::hal::vdp1::push_polygon(req);
}

extern "C" sat_result_t sat_draw_world_sprite(
    const sat_mat4_t* view_proj,
    const sat_quad3_t* quad,
    const sat_texture_t* texture,
    uint16_t palette_override,
    uint16_t flags
) {
    if (texture == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    sat_quad2_t projected = {};
    const sat_result_t st = project(view_proj, quad, &projected);
    if (st != SAT_OK) {
        return st;
    }

    sat_distorted_sprite_cmd_t cmd = {};
    for (int i = 0; i < 4; ++i) {
        cmd.x[i] = projected.x[i];
        cmd.y[i] = projected.y[i];
    }
    cmd.texture = texture;
    cmd.palette_override = palette_override;
    cmd.flags = flags;
    return sat_draw_sprite_distorted(&cmd);
}

extern "C" void sat_sort_indices_desc(uint8_t* indices, const uint32_t* keys, uint16_t count) {
    sort_indices_desc(indices, keys, count);
}

extern "C" void sat_sort_indices16_desc(uint16_t* indices, const uint32_t* keys, uint32_t count) {
    sort_indices16_desc(indices, keys, count);
}

extern "C" uint32_t sat_ground_distance_sq(
    sat_fx16_t ax,
    sat_fx16_t az,
    sat_fx16_t bx,
    sat_fx16_t bz
) {
    return ground_distance_sq(ax, az, bx, bz);
}

extern "C" uint16_t sat_shade_rgb555(uint16_t rgb555, sat_fx16_t intensity) {
    return shade_rgb555(rgb555, intensity);
}

extern "C" sat_fx16_t sat_face_intensity(
    sat_fx16_t nx,
    sat_fx16_t nz,
    sat_fx16_t floor_intensity
) {
    return face_intensity(nx, nz, floor_intensity);
}

extern "C" sat_fx16_t sat_face_intensity3(const sat_vec3_t* normal, sat_fx16_t floor_intensity) {
    if (normal == nullptr) {
        return 0;
    }
    return saturn::core::render3d::face_intensity3(*normal, floor_intensity);
}

extern "C" sat_fx16_t sat_face_intensity3_scaled(const sat_vec3_t* normal, sat_fx16_t floor_intensity) {
    if (normal == nullptr) {
        return 0;
    }
    return saturn::core::render3d::face_intensity3_scaled(*normal, floor_intensity);
}
