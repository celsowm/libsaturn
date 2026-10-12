#include "saturn/skin3d.h"

namespace {

constexpr int32_t kWeightOne = 32768;

/* M * q for one joint-local position. `scale` = 1 << (16 - pos_shift) turns the
 * int16 position into 16.16 with one multiply (exact: |q * scale| <= 2^31 - 2^16),
 * so the 64-bit accumulate is shifted by the constant 16 -- inline on the SH-2 --
 * instead of a variable 64-bit shift library call. Rounded to nearest so a blend
 * of two influences stays within one 16.16 LSB. */
inline void transform(const sat_fx16_t* m, const int16_t* q, int32_t scale, int32_t out[3]) {
    const int64_t x = q[0] * scale, y = q[1] * scale, z = q[2] * scale;
    out[0] = (int32_t)(((int64_t)m[0] * x + (int64_t)m[1] * y + (int64_t)m[2] * z + 32768) >> 16) + m[3];
    out[1] = (int32_t)(((int64_t)m[4] * x + (int64_t)m[5] * y + (int64_t)m[6] * z + 32768) >> 16) + m[7];
    out[2] = (int32_t)(((int64_t)m[8] * x + (int64_t)m[9] * y + (int64_t)m[10] * z + 32768) >> 16) + m[11];
}

inline void deform_rigid(const sat_skin_asset_t& s, const sat_mat4_t* mats, int32_t scale,
                         uint32_t v, sat_vec3_t* out) {
    const sat_skin_rigid_t& r = s.rigid[v];
    int32_t p[3];
    transform(mats[r.joint].m, r.pos, scale, p);
    out->x = p[0];
    out->y = p[1];
    out->z = p[2];
}

inline void deform_blend(const sat_skin_asset_t& s, const sat_mat4_t* mats, int32_t scale,
                         uint32_t b, sat_vec3_t* out) {
    const sat_skin_blend_t& r = s.blend[b];
    int32_t p0[3], p1[3];
    transform(mats[r.joint[0]].m, r.pos[0], scale, p0);
    if (r.count <= 1) {
        out->x = p0[0]; out->y = p0[1]; out->z = p0[2];
        return;
    }
    transform(mats[r.joint[1]].m, r.pos[1], scale, p1);
    if (r.count == 2) {
        if (r.weight[0] == kWeightOne / 2) { /* exact half: no weight multiplies */
            out->x = (int32_t)(((int64_t)p0[0] + p1[0]) >> 1);
            out->y = (int32_t)(((int64_t)p0[1] + p1[1]) >> 1);
            out->z = (int32_t)(((int64_t)p0[2] + p1[2]) >> 1);
            return;
        }
        const int64_t w0 = r.weight[0], w1 = kWeightOne - w0;
        out->x = (int32_t)((w0 * p0[0] + w1 * p1[0] + (kWeightOne >> 1)) >> 15);
        out->y = (int32_t)((w0 * p0[1] + w1 * p1[1] + (kWeightOne >> 1)) >> 15);
        out->z = (int32_t)((w0 * p0[2] + w1 * p1[2] + (kWeightOne >> 1)) >> 15);
        return;
    }
    int32_t p2[3];
    transform(mats[r.joint[2]].m, r.pos[2], scale, p2);
    const int64_t w0 = r.weight[0], w1 = r.weight[1], w2 = kWeightOne - w0 - w1;
    out->x = (int32_t)((w0 * p0[0] + w1 * p1[0] + w2 * p2[0] + (kWeightOne >> 1)) >> 15);
    out->y = (int32_t)((w0 * p0[1] + w1 * p1[1] + w2 * p2[1] + (kWeightOne >> 1)) >> 15);
    out->z = (int32_t)((w0 * p0[2] + w1 * p1[2] + w2 * p2[2] + (kWeightOne >> 1)) >> 15);
}

sat_result_t check_args(const sat_skin_asset_t* skin, const sat_mat4_t* mats, uint16_t mat_count,
                        uint32_t out_capacity, const void* out) {
    if (!skin || !mats || !out) return SAT_ERR_INVALID_ARG;
    if ((skin->rigid_count && !skin->rigid) || (skin->blend_count && !skin->blend))
        return SAT_ERR_INVALID_ARG;
    if (skin->pos_shift > 15) return SAT_ERR_INVALID_ARG;
    if ((skin->rigid_count || skin->blend_count) && skin->max_joint >= mat_count)
        return SAT_ERR_INVALID_ARG;
    if (out_capacity < sat_skin_vertex_count(skin)) return SAT_ERR_CAPACITY;
    return SAT_OK;
}

}  // namespace

extern "C" uint32_t sat_skin_vertex_count(const sat_skin_asset_t* skin) {
    return skin ? (uint32_t)skin->rigid_count + skin->blend_count : 0;
}

extern "C" sat_result_t sat_skin_validate(const sat_skin_asset_t* skin) {
    if (!skin || skin->pos_shift > 15) return SAT_ERR_INVALID_ARG;
    if ((skin->rigid_count && !skin->rigid) || (skin->blend_count && !skin->blend))
        return SAT_ERR_INVALID_ARG;
    uint8_t highest = 0;
    for (uint16_t i = 0; i < skin->rigid_count; ++i) {
        if (skin->rigid[i].joint >= skin->joint_count) return SAT_ERR_INVALID_ARG;
        if (skin->rigid[i].joint > highest) highest = skin->rigid[i].joint;
    }
    for (uint16_t i = 0; i < skin->blend_count; ++i) {
        const sat_skin_blend_t& b = skin->blend[i];
        if (b.count < 1 || b.count > 3) return SAT_ERR_INVALID_ARG;
        for (uint8_t k = 0; k < b.count; ++k) {
            if (b.joint[k] >= skin->joint_count) return SAT_ERR_INVALID_ARG;
            if (b.joint[k] > highest) highest = b.joint[k];
        }
        if (b.count >= 2 && b.weight[0] > kWeightOne) return SAT_ERR_INVALID_ARG;
        if (b.count == 3 && (uint32_t)b.weight[0] + b.weight[1] > (uint32_t)kWeightOne)
            return SAT_ERR_INVALID_ARG;
    }
    if ((skin->rigid_count || skin->blend_count) && skin->max_joint != highest)
        return SAT_ERR_INVALID_ARG;
    return SAT_OK;
}

extern "C" sat_result_t sat_skeleton_world_matrices(
    const sat_skeleton_instance_t* instance, sat_mat4_t* out, uint16_t capacity) {
    if (!instance || !instance->skeleton || !out) return SAT_ERR_INVALID_ARG;
    const uint16_t n = instance->skeleton->joint_count;
    if (capacity < n) return SAT_ERR_CAPACITY;
    if (!instance->evaluated) return SAT_ERR_BUSY;
    for (uint16_t i = 0; i < n; ++i)
        SAT_TRY(sat_transform3d_get_world(&instance->world, i, &out[i]));
    return SAT_OK;
}

extern "C" sat_result_t sat_skin_deform_range(
    const sat_skin_asset_t* skin, const sat_mat4_t* joint_world, uint16_t joint_world_count,
    uint32_t first, uint32_t count, sat_vec3_t* out, uint32_t out_capacity) {
    SAT_TRY(check_args(skin, joint_world, joint_world_count, out_capacity, out));
    const uint32_t total = sat_skin_vertex_count(skin);
    if (first > total || count > total - first) return SAT_ERR_INVALID_ARG;
    const uint32_t end = first + count;
    const int32_t scale = (int32_t)1 << (16 - skin->pos_shift);
    uint32_t v = first;
    for (; v < end && v < skin->rigid_count; ++v) deform_rigid(*skin, joint_world, scale, v, &out[v]);
    for (; v < end; ++v) deform_blend(*skin, joint_world, scale, v - skin->rigid_count, &out[v]);
    return SAT_OK;
}

extern "C" sat_result_t sat_skin_deform(
    const sat_skin_asset_t* skin, const sat_mat4_t* joint_world, uint16_t joint_world_count,
    sat_vec3_t* out, uint32_t out_capacity) {
    return sat_skin_deform_range(skin, joint_world, joint_world_count, 0,
                                 sat_skin_vertex_count(skin), out, out_capacity);
}

extern "C" sat_result_t sat_skin_deform_normals(
    const sat_skin_asset_t* skin, const sat_mat4_t* joint_world, uint16_t joint_world_count,
    sat_vec3_t* out, uint32_t out_capacity) {
    SAT_TRY(check_args(skin, joint_world, joint_world_count, out_capacity, out));
    if (!skin->normals) return SAT_ERR_UNSUPPORTED;
    const uint32_t total = sat_skin_vertex_count(skin);
    for (uint32_t v = 0; v < total; ++v) {
        const uint8_t joint = v < skin->rigid_count ? skin->rigid[v].joint
                                                   : skin->blend[v - skin->rigid_count].joint[0];
        const sat_fx16_t* m = joint_world[joint].m;
        const int64_t x = skin->normals[v * 3], y = skin->normals[v * 3 + 1],
                      z = skin->normals[v * 3 + 2];
        out[v].x = (int32_t)(((int64_t)m[0] * x + (int64_t)m[1] * y + (int64_t)m[2] * z) >> 14);
        out[v].y = (int32_t)(((int64_t)m[4] * x + (int64_t)m[5] * y + (int64_t)m[6] * z) >> 14);
        out[v].z = (int32_t)(((int64_t)m[8] * x + (int64_t)m[9] * y + (int64_t)m[10] * z) >> 14);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_skin_bounds(
    const sat_vec3_t* vertices, uint32_t count, sat_fx16_t margin, sat_aabb3_t* out) {
    if (!vertices || !out || count == 0 || margin < 0) return SAT_ERR_INVALID_ARG;
    sat_vec3_t lo = vertices[0], hi = vertices[0];
    for (uint32_t i = 1; i < count; ++i) {
        const sat_vec3_t& v = vertices[i];
        if (v.x < lo.x) lo.x = v.x;
        if (v.y < lo.y) lo.y = v.y;
        if (v.z < lo.z) lo.z = v.z;
        if (v.x > hi.x) hi.x = v.x;
        if (v.y > hi.y) hi.y = v.y;
        if (v.z > hi.z) hi.z = v.z;
    }
    out->center.x = (sat_fx16_t)(((int64_t)lo.x + hi.x) >> 1);
    out->center.y = (sat_fx16_t)(((int64_t)lo.y + hi.y) >> 1);
    out->center.z = (sat_fx16_t)(((int64_t)lo.z + hi.z) >> 1);
    out->half.x = (sat_fx16_t)((((int64_t)hi.x - lo.x + 1) >> 1) + margin);
    out->half.y = (sat_fx16_t)((((int64_t)hi.y - lo.y + 1) >> 1) + margin);
    out->half.z = (sat_fx16_t)((((int64_t)hi.z - lo.z + 1) >> 1) + margin);
    return SAT_OK;
}

extern "C" sat_result_t sat_skin_validate_mesh(
    const sat_skin_asset_t* skin, const sat_mesh_t* mesh) {
    if (!skin || !mesh || !mesh->vertices || !mesh->indices) return SAT_ERR_INVALID_ARG;
    const uint32_t n = sat_skin_vertex_count(skin);
    if (n == 0) return SAT_ERR_INVALID_ARG;
    if (mesh->vertex_cap < n) return SAT_ERR_CAPACITY;
    if (mesh->face_count > mesh->face_cap) return SAT_ERR_INVALID_ARG;
    for (uint32_t i = 0; i < (uint32_t)mesh->face_count * 4u; ++i)
        if (mesh->indices[i] >= n) return SAT_ERR_INVALID_ARG;
    return SAT_OK;
}

extern "C" sat_result_t sat_skin_update_mesh(
    const sat_skin_asset_t* skin, const sat_mat4_t* joint_world, uint16_t joint_world_count,
    sat_mesh_t* mesh, sat_fx16_t margin, sat_aabb3_t* bounds_out) {
    if (!skin || !mesh || !mesh->vertices || margin < 0) return SAT_ERR_INVALID_ARG;
    const uint32_t n = sat_skin_vertex_count(skin);
    if (n == 0) return SAT_ERR_INVALID_ARG;
    if (mesh->vertex_cap < n) return SAT_ERR_CAPACITY;
    SAT_TRY(sat_skin_deform(skin, joint_world, joint_world_count, mesh->vertices, mesh->vertex_cap));
    mesh->vertex_count = (uint16_t)n;
    if (bounds_out) SAT_TRY(sat_skin_bounds(mesh->vertices, n, margin, bounds_out));
    return SAT_OK;
}
