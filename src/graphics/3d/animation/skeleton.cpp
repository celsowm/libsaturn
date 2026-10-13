#include "saturn/skeleton3d.h"

namespace {

constexpr int32_t kOne = SAT_QUAT16_ONE;

uint32_t isqrt32(uint32_t v) {
    uint32_t r = 0;
    uint32_t bit = 1u << 30;
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
    return r;
}

int16_t clamp16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

/* Divides with rounding to nearest, ties away from zero. */
int32_t div_round(int32_t num, int32_t den) {
    const int32_t half = den >> 1;
    return (num >= 0) ? (num + half) / den : -((-num + half) / den);
}

sat_fx16_t clamp_t(sat_fx16_t t) {
    if (t < 0) return 0;
    if (t > 65536) return 65536;
    return t;
}

bool unit_ish(const sat_quat16_t& q) {
    const int32_t x = q.x, y = q.y, z = q.z, w = q.w;
    /* Q30 squared length of a unit quaternion is 2^30; allow ~3%. */
    const int64_t n = (int64_t)x * x + (int64_t)y * y + (int64_t)z * z + (int64_t)w * w;
    const int64_t one = (int64_t)1 << 30;
    return n > one - (one >> 5) && n < one + (one >> 5);
}

void rigid_inverse(sat_mat4_t* out, const sat_mat4_t& m) {
    sat_mat4_t r{};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) r.m[row * 4 + col] = m.m[col * 4 + row];
    for (int row = 0; row < 3; ++row) {
        r.m[row * 4 + 3] = -(sat_fx16_mul(r.m[row * 4 + 0], m.m[3]) +
                             sat_fx16_mul(r.m[row * 4 + 1], m.m[7]) +
                             sat_fx16_mul(r.m[row * 4 + 2], m.m[11]));
    }
    r.m[15] = 65536;
    *out = r;
}

/* A scale of zero means "uninitialised" (struct {} zero-init); treat as 1.0
 * so legacy assets and host tests that don't set rest_scale still work. */
sat_vec3_t scale_or_identity(const sat_vec3_t& s) {
    if (s.x == 0 && s.y == 0 && s.z == 0) {
        return {SAT_FX16_ONE, SAT_FX16_ONE, SAT_FX16_ONE};
    }
    return s;
}

/* Build the local matrix M_local = T * R * S. The translation row index is
 * column 3 in row-major storage (m[3], m[7], m[11]); the bottom row (m[12..15])
 * is left as the homogeneous affine row. Scale is per-component 16.16 so
 * non-uniform (sx, sy, sz) is honoured exactly. */
void joint_local_matrix(sat_mat4_t* out, const sat_skeleton_joint_t& j,
                        const sat_skeleton_pose_joint_t& pose) {
    sat_quat16_to_mat4(out, &pose.rotation, &pose.translation);
    const sat_vec3_t scale = scale_or_identity(j.rest_scale);
    out->m[0]  = sat_fx16_mul(out->m[0],  scale.x);
    out->m[1]  = sat_fx16_mul(out->m[1],  scale.y);
    out->m[2]  = sat_fx16_mul(out->m[2],  scale.z);
    out->m[4]  = sat_fx16_mul(out->m[4],  scale.x);
    out->m[5]  = sat_fx16_mul(out->m[5],  scale.y);
    out->m[6]  = sat_fx16_mul(out->m[6],  scale.z);
    out->m[8]  = sat_fx16_mul(out->m[8],  scale.x);
    out->m[9]  = sat_fx16_mul(out->m[9],  scale.y);
    out->m[10] = sat_fx16_mul(out->m[10], scale.z);
}

/* Affine 4x4 inverse via the 3x3 cofactor of the upper-left block and a
 * 64-bit translation term. Caller guarantees the bottom row is
 * [0, 0, 0, SAT_FX16_ONE] (the canonical affine form the rest of the
 * library produces); singular matrices return SAT_ERR_INVALID_ARG. */
sat_result_t affine_inverse(sat_mat4_t* out, const sat_mat4_t& m) {
    const int64_t a00 = m.m[0], a01 = m.m[1], a02 = m.m[2];
    const int64_t a10 = m.m[4], a11 = m.m[5], a12 = m.m[6];
    const int64_t a20 = m.m[8], a21 = m.m[9], a22 = m.m[10];
    const int64_t c00 = a11 * a22 - a12 * a21;
    const int64_t c01 = a12 * a20 - a10 * a22;
    const int64_t c02 = a10 * a21 - a11 * a20;
    const int64_t det = a00 * c00 + a01 * c01 + a02 * c02;
    if (det == 0) return SAT_ERR_INVALID_ARG;
    const int64_t c10 = a02 * a21 - a01 * a22;
    const int64_t c11 = a00 * a22 - a02 * a20;
    const int64_t c12 = a01 * a20 - a00 * a21;
    const int64_t c20 = a01 * a12 - a02 * a11;
    const int64_t c21 = a02 * a10 - a00 * a12;
    const int64_t c22 = a00 * a11 - a01 * a10;
    /* A^-1 = transpose(C) / det, packed into row-major 4x4. */
    constexpr int64_t s = (int64_t)1 << 16;
    out->m[0]  = (sat_fx16_t)(c00 * s / det);
    out->m[1]  = (sat_fx16_t)(c10 * s / det);
    out->m[2]  = (sat_fx16_t)(c20 * s / det);
    out->m[4]  = (sat_fx16_t)(c01 * s / det);
    out->m[5]  = (sat_fx16_t)(c11 * s / det);
    out->m[6]  = (sat_fx16_t)(c21 * s / det);
    out->m[8]  = (sat_fx16_t)(c02 * s / det);
    out->m[9]  = (sat_fx16_t)(c12 * s / det);
    out->m[10] = (sat_fx16_t)(c22 * s / det);
    /* Translation column: -A^-1 * t. The dominant magnitude is sum of
     * three fx16 products per axis (well inside int64) so no overflow. */
    const int64_t tx = -(out->m[0]  * (int64_t)m.m[3] +
                          out->m[4]  * (int64_t)m.m[7] +
                          out->m[8]  * (int64_t)m.m[11]);
    const int64_t ty = -(out->m[1]  * (int64_t)m.m[3] +
                          out->m[5]  * (int64_t)m.m[7] +
                          out->m[9]  * (int64_t)m.m[11]);
    const int64_t tz = -(out->m[2]  * (int64_t)m.m[3] +
                          out->m[6]  * (int64_t)m.m[7] +
                          out->m[10] * (int64_t)m.m[11]);
    out->m[3]  = (sat_fx16_t)(tx >> 16);
    out->m[7]  = (sat_fx16_t)(ty >> 16);
    out->m[11] = (sat_fx16_t)(tz >> 16);
    out->m[12] = out->m[13] = out->m[14] = 0;
    out->m[15] = SAT_FX16_ONE;
    return SAT_OK;
}

}  // namespace

extern "C" void sat_quat16_identity(sat_quat16_t* out) {
    if (!out) return;
    out->x = out->y = out->z = 0;
    out->w = 32767;
}

extern "C" void sat_quat16_normalize(sat_quat16_t* out, const sat_quat16_t* q) {
    if (!out || !q) return;
    const int32_t x = q->x, y = q->y, z = q->z, w = q->w;
    /* Q30 squared length fits uint32 except all four at -32768, which wraps
     * to zero and is a length of exactly 2.0 (65536 in Q15). */
    const uint32_t len2 = (uint32_t)(x * x) + (uint32_t)(y * y) +
                          (uint32_t)(z * z) + (uint32_t)(w * w);
    const bool all_max = (x == -32768 && y == -32768 && z == -32768 && w == -32768);
    const int32_t n = all_max ? 65536 : (int32_t)isqrt32(len2); /* Q15 length */
    if (n == 0) {
        sat_quat16_identity(out);
        return;
    }
    out->x = clamp16(div_round(x * kOne, n));
    out->y = clamp16(div_round(y * kOne, n));
    out->z = clamp16(div_round(z * kOne, n));
    out->w = clamp16(div_round(w * kOne, n));
}

extern "C" void sat_quat16_nlerp(
    sat_quat16_t* out, const sat_quat16_t* a, const sat_quat16_t* b,
    sat_fx16_t t) {
    if (!out || !a || !b) return;
    const sat_quat16_t qa = *a, qb = *b;
    const int32_t dot = ((qa.x * qb.x) >> 2) + ((qa.y * qb.y) >> 2) +
                        ((qa.z * qb.z) >> 2) + ((qa.w * qb.w) >> 2);
    const int32_t sign = dot < 0 ? -1 : 1;
    const int32_t w1 = clamp_t(t) >> 1; /* Q15 */
    const int32_t w0 = kOne - w1;
    const int32_t in_a[4] = {qa.x, qa.y, qa.z, qa.w};
    const int32_t in_b[4] = {qb.x, qb.y, qb.z, qb.w};
    int32_t r[4];
    for (int i = 0; i < 4; ++i) {
        const int32_t sum = ((in_a[i] * w0) >> 1) + (((sign * in_b[i]) * w1) >> 1);
        r[i] = (sum + (1 << 13)) >> 14;
    }
    sat_quat16_t blended = {clamp16(r[0]), clamp16(r[1]), clamp16(r[2]), clamp16(r[3])};
    sat_quat16_normalize(out, &blended);
}

extern "C" void sat_quat16_to_mat4(
    sat_mat4_t* out, const sat_quat16_t* q, const sat_vec3_t* translation) {
    if (!out || !q) return;
    const int32_t x = q->x, y = q->y, z = q->z, w = q->w;
    const int32_t xx = x * x, yy = y * y, zz = z * z;
    const int32_t xy = x * y, xz = x * z, yz = y * z;
    const int32_t xw = x * w, yw = y * w, zw = z * w;
    sat_fx16_t* m = out->m;
    m[0] = 65536 - ((yy + zz) >> 13);
    m[1] = (xy - zw) >> 13;
    m[2] = (xz + yw) >> 13;
    m[3] = translation ? translation->x : 0;
    m[4] = (xy + zw) >> 13;
    m[5] = 65536 - ((xx + zz) >> 13);
    m[6] = (yz - xw) >> 13;
    m[7] = translation ? translation->y : 0;
    m[8] = (xz - yw) >> 13;
    m[9] = (yz + xw) >> 13;
    m[10] = 65536 - ((xx + yy) >> 13);
    m[11] = translation ? translation->z : 0;
    m[12] = m[13] = m[14] = 0;
    m[15] = 65536;
}

extern "C" sat_result_t sat_skeleton_validate(const sat_skeleton_asset_t* skeleton) {
    if (!skeleton || !skeleton->joints || skeleton->joint_count == 0 ||
        skeleton->joint_count >= SAT_TRANSFORM3D_ROOT)
        return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < skeleton->joint_count; ++i) {
        const sat_skeleton_joint_t& j = skeleton->joints[i];
        if (j.parent != SAT_TRANSFORM3D_ROOT && j.parent >= i) return SAT_ERR_INVALID_ARG;
        if (!unit_ish(j.rest_rotation)) return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_skeleton_rest_pose(
    const sat_skeleton_asset_t* skeleton, sat_skeleton_pose_joint_t* out,
    uint16_t capacity) {
    SAT_TRY(sat_skeleton_validate(skeleton));
    if (!out) return SAT_ERR_INVALID_ARG;
    if (capacity < skeleton->joint_count) return SAT_ERR_CAPACITY;
    for (uint16_t i = 0; i < skeleton->joint_count; ++i) {
        out[i].translation = skeleton->joints[i].rest_translation;
        out[i].rotation = skeleton->joints[i].rest_rotation;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_skeleton_pose_blend(
    sat_skeleton_pose_joint_t* out, const sat_skeleton_pose_joint_t* a,
    const sat_skeleton_pose_joint_t* b, uint16_t joint_count, sat_fx16_t t) {
    if (!out || !a || !b) return SAT_ERR_INVALID_ARG;
    const sat_fx16_t tc = clamp_t(t);
    for (uint16_t i = 0; i < joint_count; ++i) {
        const sat_skeleton_pose_joint_t ja = a[i], jb = b[i];
        out[i].translation.x = ja.translation.x + sat_fx16_mul(jb.translation.x - ja.translation.x, tc);
        out[i].translation.y = ja.translation.y + sat_fx16_mul(jb.translation.y - ja.translation.y, tc);
        out[i].translation.z = ja.translation.z + sat_fx16_mul(jb.translation.z - ja.translation.z, tc);
        sat_quat16_nlerp(&out[i].rotation, &ja.rotation, &jb.rotation, tc);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_skeleton_instance_init(
    sat_skeleton_instance_t* instance, const sat_skeleton_asset_t* skeleton,
    sat_transform3d_node_t* nodes, uint16_t node_capacity, uint16_t* scratch,
    uint16_t scratch_capacity) {
    if (!instance || !nodes || !scratch) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_skeleton_validate(skeleton));
    if (node_capacity < skeleton->joint_count ||
        scratch_capacity < skeleton->joint_count)
        return SAT_ERR_CAPACITY;
    sat_transform3d_world_t world{};
    SAT_TRY(sat_transform3d_world_init(&world, nodes, node_capacity));
    for (uint16_t i = 0; i < skeleton->joint_count; ++i) {
        uint16_t id = 0;
        SAT_TRY(sat_transform3d_create(&world, &id));
        SAT_TRY(sat_transform3d_set_parent(&world, id, skeleton->joints[i].parent));
    }
    instance->skeleton = skeleton;
    instance->world = world;
    instance->scratch = scratch;
    instance->scratch_capacity = scratch_capacity;
    instance->evaluated = 0;
    instance->reserved = 0;
    return SAT_OK;
}

extern "C" sat_result_t sat_skeleton_evaluate(
    sat_skeleton_instance_t* instance, const sat_skeleton_pose_joint_t* pose,
    uint16_t pose_count) {
    if (!instance || !instance->skeleton || !pose) return SAT_ERR_INVALID_ARG;
    const uint16_t n = instance->skeleton->joint_count;
    if (pose_count != n) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < n; ++i) {
        sat_mat4_t local{};
        joint_local_matrix(&local, instance->skeleton->joints[i], pose[i]);
        SAT_TRY(sat_transform3d_set_local_matrix(&instance->world, i, &local));
    }
    SAT_TRY(sat_transform3d_evaluate(&instance->world, instance->scratch,
                                     instance->scratch_capacity));
    instance->evaluated = 1;
    return SAT_OK;
}

extern "C" sat_result_t sat_skeleton_joint_world(
    const sat_skeleton_instance_t* instance, uint16_t joint, sat_mat4_t* out) {
    if (!instance || !instance->skeleton || !out ||
        joint >= instance->skeleton->joint_count)
        return SAT_ERR_INVALID_ARG;
    if (!instance->evaluated) return SAT_ERR_BUSY;
    return sat_transform3d_get_world(&instance->world, joint, out);
}

extern "C" sat_result_t sat_skeleton_inverse_bind(
    const sat_skeleton_asset_t* skeleton, sat_mat4_t* out, uint16_t capacity) {
    SAT_TRY(sat_skeleton_validate(skeleton));
    if (!out) return SAT_ERR_INVALID_ARG;
    if (capacity < skeleton->joint_count) return SAT_ERR_CAPACITY;
    /* Build the rest-pose world matrices including each joint's per-component
     * `rest_scale`, then take the affine inverse (rigid inverse is only
     * correct when every scale component is 1). The transform hierarchy and
     * matrix-multiply path use the library's existing T*R composition and
     * require an affine bottom row, which `joint_local_matrix` guarantees. */
    for (uint16_t i = 0; i < skeleton->joint_count; ++i) {
        const sat_skeleton_joint_t& j = skeleton->joints[i];
        sat_skeleton_pose_joint_t rest_pose{};
        rest_pose.translation = j.rest_translation;
        rest_pose.rotation = j.rest_rotation;
        sat_mat4_t local{};
        joint_local_matrix(&local, j, rest_pose);
        if (j.parent == SAT_TRANSFORM3D_ROOT) out[i] = local;
        else SAT_TRY(sat_mat4_multiply(&out[i], &out[j.parent], &local));
    }
    for (uint16_t i = 0; i < skeleton->joint_count; ++i) {
        SAT_TRY(affine_inverse(&out[i], out[i]));
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_skeleton_skin_matrices(
    const sat_skeleton_instance_t* instance, const sat_mat4_t* inverse_bind,
    sat_mat4_t* out, uint16_t capacity) {
    if (!instance || !instance->skeleton || !inverse_bind || !out)
        return SAT_ERR_INVALID_ARG;
    const uint16_t n = instance->skeleton->joint_count;
    if (capacity < n) return SAT_ERR_CAPACITY;
    if (!instance->evaluated) return SAT_ERR_BUSY;
    for (uint16_t i = 0; i < n; ++i) {
        sat_mat4_t world{};
        SAT_TRY(sat_transform3d_get_world(&instance->world, i, &world));
        SAT_TRY(sat_mat4_multiply(&out[i], &world, &inverse_bind[i]));
    }
    return SAT_OK;
}
