#include "saturn/skin3d.h"

namespace {

uint32_t isqrt64(uint64_t v) {
    uint64_t r = 0;
    uint64_t bit = (uint64_t)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)r;
}

int64_t abs64(int64_t v) { return v < 0 ? -v : v; }

/* Scales a vector so its largest component fits 20 bits, keeping direction. */
void fit20(int64_t v[3]) {
    int64_t m = abs64(v[0]);
    if (abs64(v[1]) > m) m = abs64(v[1]);
    if (abs64(v[2]) > m) m = abs64(v[2]);
    int shift = 0;
    while ((m >> shift) >= ((int64_t)1 << 20)) ++shift;
    if (shift) {
        v[0] >>= shift;
        v[1] >>= shift;
        v[2] >>= shift;
    }
}

}  // namespace

extern "C" sat_result_t sat_skin_requirements(const sat_skeleton_asset_t* skeleton,
    const sat_skin_asset_t* skin, uint16_t faces, sat_skin_requirements_t* out) {
    if (!skeleton || !skin || !out || skeleton->joint_count != skin->joint_count)
        return SAT_ERR_INVALID_ARG;
    const uint32_t joints = skeleton->joint_count;
    *out = {joints * static_cast<uint32_t>(sizeof(sat_skeleton_joint_t)),
        skin->rigid_count * static_cast<uint32_t>(sizeof(sat_skin_rigid_t)) + skin->blend_count * static_cast<uint32_t>(sizeof(sat_skin_blend_t)),
        sat_skin_vertex_count(skin) * static_cast<uint32_t>(sizeof(sat_vec3_t)), joints * static_cast<uint32_t>(sizeof(sat_mat4_t)),
        joints * static_cast<uint32_t>(sizeof(sat_skeleton_pose_joint_t)), joints * static_cast<uint32_t>(sizeof(sat_transform3d_node_t)),
        joints * static_cast<uint32_t>(sizeof(uint16_t)), faces};
    return SAT_OK;
}

extern "C" sat_result_t sat_skeleton_attachment(const sat_skeleton_instance_t* instance,
    uint16_t joint, const sat_mat4_t* world, const sat_mat4_t* local, sat_mat4_t* out) {
    if (!out) return SAT_ERR_INVALID_ARG;
    sat_mat4_t result;
    SAT_TRY(sat_skeleton_joint_world(instance, joint, &result));
    sat_mat4_t product;
    if (local) { SAT_TRY(sat_mat4_multiply(&product, &result, local)); result = product; }
    if (world) { SAT_TRY(sat_mat4_multiply(&product, world, &result)); result = product; }
    *out = result;
    return SAT_OK;
}

extern "C" sat_result_t sat_mesh_face_view(
    const sat_mesh_t* mesh, sat_face_range_t range, sat_mesh_t* out_view) {
    if (!mesh || !out_view || !mesh->indices) return SAT_ERR_INVALID_ARG;
    if ((uint32_t)range.first_face + range.face_count > mesh->face_count)
        return SAT_ERR_INVALID_ARG;
    out_view->vertices = mesh->vertices;
    out_view->indices = mesh->indices + (uint32_t)range.first_face * 4u;
    out_view->vertex_cap = mesh->vertex_cap;
    out_view->vertex_count = mesh->vertex_count;
    out_view->face_cap = range.face_count;
    out_view->face_count = range.face_count;
    return SAT_OK;
}

extern "C" sat_result_t sat_lod_select(
    const sat_face_range_t* levels, uint16_t level_count, uint16_t face_budget,
    uint16_t* out_level) {
    if (!levels || level_count == 0 || !out_level) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < level_count; ++i) {
        if (levels[i].face_count <= face_budget) {
            *out_level = i;
            return SAT_OK;
        }
    }
    *out_level = (uint16_t)(level_count - 1);
    return SAT_ERR_CAPACITY;
}

extern "C" sat_result_t sat_mesh_face_ramp(
    const sat_mesh_t* mesh, const sat_vec3_t* light_dir, sat_fx16_t ambient,
    uint16_t ramp_count, uint16_t* out_face_materials, uint16_t capacity) {
    if (!mesh || !mesh->vertices || !mesh->indices || !light_dir || !out_face_materials ||
        ramp_count < 2 || ambient < 0 || ambient > 65536)
        return SAT_ERR_INVALID_ARG;
    if (capacity < mesh->face_count) return SAT_ERR_CAPACITY;
    for (uint16_t i = 0; i < mesh->face_count * 4u; ++i)
        if (mesh->indices[i] >= mesh->vertex_count) return SAT_ERR_INVALID_ARG;

    int64_t l[3] = {light_dir->x, light_dir->y, light_dir->z};
    fit20(l);
    const uint32_t llen = isqrt64((uint64_t)(l[0] * l[0] + l[1] * l[1] + l[2] * l[2]));
    if (llen == 0) return SAT_ERR_INVALID_ARG;

    const int32_t span = ramp_count - 1;
    for (uint16_t f = 0; f < mesh->face_count; ++f) {
        const uint16_t* q = mesh->indices + (uint32_t)f * 4u;
        const sat_vec3_t& a = mesh->vertices[q[0]];
        const sat_vec3_t& b = mesh->vertices[q[1]];
        const sat_vec3_t& d = mesh->vertices[q[3]];
        const int64_t u[3] = {(int64_t)d.x - a.x, (int64_t)d.y - a.y, (int64_t)d.z - a.z};
        const int64_t w[3] = {(int64_t)b.x - a.x, (int64_t)b.y - a.y, (int64_t)b.z - a.z};
        int64_t n[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2],
                        u[0] * w[1] - u[1] * w[0]};
        fit20(n);
        const uint32_t nlen = isqrt64((uint64_t)(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]));
        int64_t ndotl = 0; /* 16.16 */
        if (nlen != 0) {
            const int64_t dot = n[0] * l[0] + n[1] * l[1] + n[2] * l[2];
            ndotl = (dot * 65536) / ((int64_t)nlen * llen);
        }
        if (ndotl < 0) ndotl = 0;
        if (ndotl > 65536) ndotl = 65536;
        /* intensity = ambient + (1 - ambient) * ndotl, all 16.16 */
        const int64_t intensity = ambient + (((int64_t)(65536 - ambient) * ndotl) >> 16);
        out_face_materials[f] = (uint16_t)((intensity * span + 32768) >> 16);
    }
    return SAT_OK;
}
