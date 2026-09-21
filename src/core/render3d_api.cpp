#include "saturn/render3d.h"

#include "src/core/render3d_logic.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/vdp1.hpp"

using namespace saturn::core::render3d;

namespace {

sat_result_t project(const sat_mat4_t* view_proj, const sat_quad3_t* quad, sat_quad2_t* out) {
    const sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) return st;
    if (view_proj == nullptr || quad == nullptr || out == nullptr) return SAT_ERR_INVALID_ARG;
    const sat_video_config_t& cfg = saturn::core::g_state.config;
    if (!project_quad(view_proj->m, quad, static_cast<int16_t>(cfg.width), static_cast<int16_t>(cfg.height), out)) {
        return SAT_ERR_UNSUPPORTED;
    }
    return SAT_OK;
}

}  // namespace

extern "C" void sat_quad3_wall(sat_quad3_t* out, sat_fx16_t x0, sat_fx16_t z0, sat_fx16_t x1, sat_fx16_t z1, sat_fx16_t height) {
    make_wall(out, x0, z0, x1, z1, height);
}
extern "C" void sat_quad3_floor(sat_quad3_t* out, sat_fx16_t cx, sat_fx16_t y, sat_fx16_t cz, sat_fx16_t half) {
    make_floor(out, cx, y, cz, half);
}
extern "C" void sat_quad3_billboard(sat_quad3_t* out, sat_fx16_t cx, sat_fx16_t cz, sat_fx16_t right_x, sat_fx16_t right_z, sat_fx16_t half_w, sat_fx16_t height) {
    make_billboard(out, cx, cz, right_x, right_z, half_w, height);
}
extern "C" int32_t sat_quad2_area2(const sat_quad2_t* quad) {
    if (quad == nullptr) return 0;
    return quad2_area2(*quad);
}
extern "C" sat_result_t sat_project_quad(const sat_mat4_t* view_proj, const sat_quad3_t* quad, sat_quad2_t* out) {
    return project(view_proj, quad, out);
}
extern "C" sat_result_t sat_clip_quad_near(
    const sat_quad3_t* quad,const sat_vec3_t* eye,
    const sat_vec3_t* forward,sat_fx16_t near_depth,
    sat_quad3_t out_triangles[4],uint8_t* out_count) {
    return saturn::core::render3d::clip_world_quad_near(
        quad,eye,forward,near_depth,out_triangles,out_count);
}
extern "C" sat_result_t sat_clip_quad_screen(
    const sat_quad2_t* quad,uint16_t width,uint16_t height,
    sat_quad2_t out_triangles[6],uint8_t* out_count) {
    return saturn::core::render3d::clip_quad_screen(
        quad,width,height,out_triangles,out_count);
}



extern "C" sat_result_t sat_draw_world_polygon(const sat_mat4_t* view_proj, const sat_quad3_t* quad, uint16_t color) {
    return sat_draw_world_polygon_effects(view_proj, quad, color, 0u);
}

extern "C" sat_result_t sat_draw_world_polygon_effects(
    const sat_mat4_t* view_proj, const sat_quad3_t* quad,
    uint16_t color, uint16_t flags) {
    sat_quad2_t projected = {};
    const sat_result_t st = project(view_proj, quad, &projected);
    if (st != SAT_OK) return st;
    return sat_draw_quad2_polygon_effects(&projected, color, flags);
}

extern "C" sat_result_t sat_draw_quad2_polygon(const sat_quad2_t* quad, uint16_t color) {
    return sat_draw_quad2_polygon_effects(quad, color, 0u);
}

extern "C" sat_result_t sat_draw_quad2_polygon_effects(
    const sat_quad2_t* quad, uint16_t color, uint16_t flags) {
    const sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) return st;
    if (quad == nullptr) return SAT_ERR_INVALID_ARG;
    saturn::hal::vdp1::PolygonRequest req = {};
    req.xa = quad->x[0]; req.ya = quad->y[0]; req.xb = quad->x[1]; req.yb = quad->y[1];
    req.xc = quad->x[2]; req.yc = quad->y[2]; req.xd = quad->x[3]; req.yd = quad->y[3];
    req.color = color; req.flags = flags;
    return saturn::hal::vdp1::push_polygon(req);
}

extern "C" sat_result_t sat_draw_quad2_sprite(const sat_quad2_t* quad, const sat_vdp1_texture_t* texture, uint16_t palette_override, uint16_t flags) {
    if (quad == nullptr || texture == nullptr) return SAT_ERR_INVALID_ARG;
    sat_distorted_sprite_cmd_t cmd = {};
    for (int i = 0; i < 4; ++i) { cmd.x[i] = quad->x[i]; cmd.y[i] = quad->y[i]; }
    cmd.texture = texture; cmd.palette_override = palette_override; cmd.flags = flags;
    return sat_draw_sprite_distorted(&cmd);
}

extern "C" sat_result_t sat_draw_quad2_polygon_gouraud(
    const sat_quad2_t* quad, uint16_t color, const uint16_t gouraud[4]) {
    return sat_draw_quad2_polygon_gouraud_effects(quad, color, gouraud, 0u);
}

extern "C" sat_result_t sat_draw_quad2_polygon_gouraud_effects(
    const sat_quad2_t* quad, uint16_t color,
    const uint16_t gouraud[4], uint16_t flags) {
    const sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) return st;
    if (quad == nullptr || gouraud == nullptr) return SAT_ERR_INVALID_ARG;
    saturn::hal::vdp1::PolygonRequest req = {};
    req.xa = quad->x[0]; req.ya = quad->y[0]; req.xb = quad->x[1]; req.yb = quad->y[1];
    req.xc = quad->x[2]; req.yc = quad->y[2]; req.xd = quad->x[3]; req.yd = quad->y[3];
    req.color = color; req.flags = flags;
    return saturn::hal::vdp1::push_polygon_gouraud(req, gouraud);
}

extern "C" sat_result_t sat_draw_world_polygon_gouraud(
    const sat_mat4_t* view_proj, const sat_quad3_t* quad,
    uint16_t color, const uint16_t gouraud[4]) {
    return sat_draw_world_polygon_gouraud_effects(view_proj, quad, color, gouraud, 0u);
}

extern "C" sat_result_t sat_draw_world_polygon_gouraud_effects(
    const sat_mat4_t* view_proj, const sat_quad3_t* quad, uint16_t color,
    const uint16_t gouraud[4], uint16_t flags) {
    if (gouraud == nullptr) return SAT_ERR_INVALID_ARG;
    sat_quad2_t projected = {};
    const sat_result_t st = project(view_proj, quad, &projected);
    if (st != SAT_OK) return st;
    return sat_draw_quad2_polygon_gouraud_effects(&projected, color, gouraud, flags);
}

extern "C" uint16_t sat_gouraud_from_intensity(sat_fx16_t intensity) { return gouraud_from_intensity(intensity); }

extern "C" sat_result_t sat_gouraud_lambert(const sat_vec3_t* normals, uint16_t count, const sat_vec3_t* light, sat_fx16_t ambient, uint16_t* out_gouraud) {
    if (light == nullptr || (count > 0u && (normals == nullptr || out_gouraud == nullptr))) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < count; ++i) out_gouraud[i] = gouraud_lambert_word(normals[i], *light, ambient);
    return SAT_OK;
}
extern "C" sat_result_t sat_gouraud_lambert_highlight(
    const sat_vec3_t* normals, uint16_t count, const sat_vec3_t* light, sat_fx16_t ambient,
    sat_fx16_t highlight_start, int32_t highlight_gain, uint16_t* out_gouraud) {
    if (light == nullptr || (count > 0u && (normals == nullptr || out_gouraud == nullptr))) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < count; ++i) {
        out_gouraud[i] = gouraud_lambert_highlight_word(
            normals[i], *light, ambient, highlight_start, highlight_gain);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_project_vertices(const sat_mat4_t* view_proj, const sat_vec3_t* points, uint16_t count, sat_projected_vertex_t* out) {
    const sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) return st;
    if (view_proj == nullptr || out == nullptr || (count > 0u && points == nullptr)) return SAT_ERR_INVALID_ARG;
    const sat_video_config_t& cfg = saturn::core::g_state.config;
    const int16_t screen_w = static_cast<int16_t>(cfg.width);
    const int16_t screen_h = static_cast<int16_t>(cfg.height);
    for (uint16_t i = 0; i < count; ++i) project_vertex(view_proj->m, points[i], screen_w, screen_h, &out[i]);
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_world_sprite(const sat_mat4_t* view_proj, const sat_quad3_t* quad, const sat_vdp1_texture_t* texture, uint16_t palette_override, uint16_t flags) {
    if (texture == nullptr) return SAT_ERR_INVALID_ARG;
    sat_quad2_t projected = {};
    const sat_result_t st = project(view_proj, quad, &projected);
    if (st != SAT_OK) return st;
    sat_distorted_sprite_cmd_t cmd = {};
    for (int i = 0; i < 4; ++i) { cmd.x[i] = projected.x[i]; cmd.y[i] = projected.y[i]; }
    cmd.texture = texture; cmd.palette_override = palette_override; cmd.flags = flags;
    return sat_draw_sprite_distorted(&cmd);
}

extern "C" void sat_sort_indices_desc(uint8_t* indices, const uint32_t* keys, uint16_t count) { sort_indices_desc(indices, keys, count); }
extern "C" void sat_sort_indices16_desc(uint16_t* indices, const uint32_t* keys, uint32_t count) { sort_indices16_desc(indices, keys, count); }
extern "C" uint32_t sat_ground_distance_sq(sat_fx16_t ax, sat_fx16_t az, sat_fx16_t bx, sat_fx16_t bz) { return ground_distance_sq(ax, az, bx, bz); }
extern "C" uint16_t sat_shade_rgb555(uint16_t rgb555, sat_fx16_t intensity) { return shade_rgb555(rgb555, intensity); }
extern "C" sat_fx16_t sat_face_intensity(sat_fx16_t nx, sat_fx16_t nz, sat_fx16_t floor_intensity) { return face_intensity(nx, nz, floor_intensity); }
extern "C" sat_fx16_t sat_face_intensity3(const sat_vec3_t* normal, sat_fx16_t floor_intensity) {
    if (normal == nullptr) return 0;
    return saturn::core::render3d::face_intensity3(*normal, floor_intensity);
}
extern "C" sat_fx16_t sat_face_intensity3_scaled(const sat_vec3_t* normal, sat_fx16_t floor_intensity) {
    if (normal == nullptr) return 0;
    return saturn::core::render3d::face_intensity3_scaled(*normal, floor_intensity);
}
