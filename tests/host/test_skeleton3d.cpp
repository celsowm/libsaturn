#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "saturn/skeleton3d.h"

#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (0)
#define FX(n) ((sat_fx16_t)((n) * 65536))

static int32_t absdiff(int32_t a, int32_t b) { return a > b ? a - b : b - a; }

static sat_quat16_t ident() { sat_quat16_t q; sat_quat16_identity(&q); return q; }

/* 90 degrees about Z: (0,0,sin45,cos45) */
static sat_quat16_t rot_z90() { return {0, 0, 23170, 23170}; }

static sat_skeleton_joint_t joint(uint16_t parent, sat_fx16_t x, sat_fx16_t y, sat_fx16_t z,
                                  sat_quat16_t q) {
    sat_skeleton_joint_t j{};
    j.parent = parent;
    j.rest_translation = {x, y, z};
    j.rest_rotation = q;
    return j;
}

static void quat_basics() {
    for (int32_t w = -32768; w <= 32767; ++w) {
        sat_quat16_t axis = {0, 0, 0, (int16_t)w};
        sat_quat16_normalize(&axis, &axis);
        CHECK(axis.x == 0 && axis.y == 0 && axis.z == 0);
        CHECK(axis.w == (w < 0 ? -32768 : 32767));
    }
    sat_mat4_t m{};
    sat_quat16_t id = ident();
    sat_vec3_t t = {FX(1), FX(2), FX(3)};
    sat_quat16_to_mat4(&m, &id, &t);
    CHECK(m.m[0] == 65536 && m.m[5] == 65536 && m.m[10] == 65536 && m.m[15] == 65536);
    CHECK(m.m[1] == 0 && m.m[2] == 0 && m.m[4] == 0);
    CHECK(m.m[3] == FX(1) && m.m[7] == FX(2) && m.m[11] == FX(3));

    sat_quat16_t z90 = rot_z90();
    sat_quat16_to_mat4(&m, &z90, nullptr);
    /* x axis maps to +y: column 0 is (0,1,0) */
    CHECK(absdiff(m.m[0], 0) < 64 && absdiff(m.m[4], 65536) < 64);
    CHECK(absdiff(m.m[1], -65536) < 64 && absdiff(m.m[5], 0) < 64);
    CHECK(m.m[10] == 65536 - ((int32_t)23170 * 23170 * 0 >> 13));

    sat_quat16_t zero = {0, 0, 0, 0}, n;
    sat_quat16_normalize(&n, &zero);
    CHECK(n.w == 32767 && n.x == 0);
    /* Small inputs lose precision to the integer square root (<= 4 LSB). */
    sat_quat16_t big = {0, 0, 1000, 1000};
    sat_quat16_normalize(&n, &big);
    CHECK(absdiff(n.z, 23170) <= 4 && absdiff(n.w, 23170) <= 4);
    /* Near-unit inputs normalise to within 1 LSB. */
    sat_quat16_t near = {0, 0, 23000, 23400};
    sat_quat16_normalize(&n, &near);
    CHECK(absdiff(n.z, 22970) <= 2 && absdiff(n.w, 23369) <= 2);
    sat_quat16_t extreme = {-32768, -32768, -32768, -32768};
    sat_quat16_normalize(&n, &extreme);
    CHECK(n.x == -16384 && n.w == -16384);
}

static void nlerp_cases() {
    sat_quat16_t a = ident(), b = rot_z90(), r;
    sat_quat16_nlerp(&r, &a, &b, 0);
    CHECK(r.z == 0 && r.w == 32767);
    sat_quat16_nlerp(&r, &a, &b, FX(1));
    CHECK(absdiff(r.z, b.z) <= 1 && absdiff(r.w, b.w) <= 1);
    /* t clamped */
    sat_quat16_nlerp(&r, &a, &b, FX(5));
    CHECK(absdiff(r.z, b.z) <= 1);
    sat_quat16_nlerp(&r, &a, &b, -FX(5));
    CHECK(r.z == 0);
    /* midpoint is 45 degrees: sin(22.5)=12540, cos(22.5)=30274 */
    sat_quat16_nlerp(&r, &a, &b, FX(1) / 2);
    CHECK(absdiff(r.z, 12540) < 400 && absdiff(r.w, 30274) < 400);
    /* shortest arc: -b is the same rotation, blend must not pass through zero */
    sat_quat16_t nb = {0, 0, (int16_t)-b.z, (int16_t)-b.w};
    sat_quat16_nlerp(&r, &a, &nb, FX(1) / 2);
    CHECK(absdiff(r.w, 30274) < 400 && r.z > 0);
    /* result is unit length */
    int64_t n2 = (int64_t)r.x * r.x + (int64_t)r.y * r.y + (int64_t)r.z * r.z + (int64_t)r.w * r.w;
    CHECK(absdiff((int32_t)(n2 >> 15), 32768) < 64);
}

static const sat_skeleton_joint_t kArm[3] = {
    joint(SAT_TRANSFORM3D_ROOT, FX(1), 0, 0, {0, 0, 0, 32767}),
    joint(0, FX(2), 0, 0, {0, 0, 0, 32767}),
    joint(1, FX(3), 0, 0, {0, 0, 0, 32767}),
};

static void validation() {
    sat_skeleton_asset_t s{kArm, 3, 0};
    CHECK(sat_skeleton_validate(&s) == SAT_OK);
    CHECK(sat_skeleton_validate(nullptr) == SAT_ERR_INVALID_ARG);
    sat_skeleton_asset_t empty{kArm, 0, 0};
    CHECK(sat_skeleton_validate(&empty) == SAT_ERR_INVALID_ARG);
    sat_skeleton_asset_t nul{nullptr, 3, 0};
    CHECK(sat_skeleton_validate(&nul) == SAT_ERR_INVALID_ARG);

    sat_skeleton_joint_t bad_parent[2] = {
        joint(SAT_TRANSFORM3D_ROOT, 0, 0, 0, ident()), joint(5, 0, 0, 0, ident())};
    sat_skeleton_asset_t b1{bad_parent, 2, 0};
    CHECK(sat_skeleton_validate(&b1) == SAT_ERR_INVALID_ARG);

    sat_skeleton_joint_t self[2] = {
        joint(SAT_TRANSFORM3D_ROOT, 0, 0, 0, ident()), joint(1, 0, 0, 0, ident())};
    sat_skeleton_asset_t b2{self, 2, 0};
    CHECK(sat_skeleton_validate(&b2) == SAT_ERR_INVALID_ARG);

    /* parent after child (would be a cycle with 0 -> 1 -> 0) */
    sat_skeleton_joint_t cyc[2] = {joint(1, 0, 0, 0, ident()), joint(0, 0, 0, 0, ident())};
    sat_skeleton_asset_t b3{cyc, 2, 0};
    CHECK(sat_skeleton_validate(&b3) == SAT_ERR_INVALID_ARG);

    sat_skeleton_joint_t nonunit[1] = {joint(SAT_TRANSFORM3D_ROOT, 0, 0, 0, {100, 0, 0, 100})};
    sat_skeleton_asset_t b4{nonunit, 1, 0};
    CHECK(sat_skeleton_validate(&b4) == SAT_ERR_INVALID_ARG);
}

static void one_bone() {
    sat_skeleton_joint_t j[1] = {joint(SAT_TRANSFORM3D_ROOT, FX(4), FX(5), FX(6), ident())};
    sat_skeleton_asset_t s{j, 1, 0};
    sat_transform3d_node_t nodes[1]{};
    uint16_t scratch[1]{};
    sat_skeleton_instance_t inst{};
    CHECK(sat_skeleton_instance_init(&inst, &s, nodes, 1, scratch, 1) == SAT_OK);
    sat_skeleton_pose_joint_t pose[1];
    CHECK(sat_skeleton_rest_pose(&s, pose, 1) == SAT_OK);
    sat_mat4_t m{};
    CHECK(sat_skeleton_joint_world(&inst, 0, &m) == SAT_ERR_BUSY);
    CHECK(sat_skeleton_evaluate(&inst, pose, 1) == SAT_OK);
    CHECK(sat_skeleton_joint_world(&inst, 0, &m) == SAT_OK);
    CHECK(m.m[3] == FX(4) && m.m[7] == FX(5) && m.m[11] == FX(6) && m.m[0] == 65536);
    CHECK(sat_skeleton_joint_world(&inst, 1, &m) == SAT_ERR_INVALID_ARG);
}

static void hierarchy_and_rotation() {
    sat_skeleton_asset_t s{kArm, 3, 0};
    sat_transform3d_node_t nodes[3]{};
    uint16_t scratch[3]{};
    sat_skeleton_instance_t inst{};
    CHECK(sat_skeleton_instance_init(&inst, &s, nodes, 2, scratch, 3) == SAT_ERR_CAPACITY);
    CHECK(sat_skeleton_instance_init(&inst, &s, nodes, 3, scratch, 2) == SAT_ERR_CAPACITY);
    CHECK(sat_skeleton_instance_init(&inst, &s, nodes, 3, scratch, 3) == SAT_OK);

    sat_skeleton_pose_joint_t pose[3];
    CHECK(sat_skeleton_rest_pose(&s, pose, 2) == SAT_ERR_CAPACITY);
    CHECK(sat_skeleton_rest_pose(&s, pose, 3) == SAT_OK);
    CHECK(sat_skeleton_evaluate(&inst, pose, 2) == SAT_ERR_INVALID_ARG);
    CHECK(sat_skeleton_evaluate(&inst, pose, 3) == SAT_OK);
    sat_mat4_t m{};
    CHECK(sat_skeleton_joint_world(&inst, 2, &m) == SAT_OK);
    CHECK(m.m[3] == FX(6) && m.m[7] == 0);

    /* rotate joint 0 by 90 deg about Z: the chain swings onto +Y */
    pose[0].rotation = rot_z90();
    CHECK(sat_skeleton_evaluate(&inst, pose, 3) == SAT_OK);
    CHECK(sat_skeleton_joint_world(&inst, 2, &m) == SAT_OK);
    CHECK(absdiff(m.m[3], FX(1)) < 256);          /* root offset (1,0,0) unaffected */
    CHECK(absdiff(m.m[7], FX(5)) < 512);          /* 2 + 3 along +Y */
    CHECK(absdiff(m.m[11], 0) < 4);

    /* determinism: re-evaluating the same pose is bit identical */
    sat_mat4_t m2{};
    CHECK(sat_skeleton_evaluate(&inst, pose, 3) == SAT_OK);
    CHECK(sat_skeleton_joint_world(&inst, 2, &m2) == SAT_OK);
    CHECK(std::memcmp(&m, &m2, sizeof m) == 0);
}

static void bind_and_skin() {
    sat_skeleton_joint_t j[3] = {
        joint(SAT_TRANSFORM3D_ROOT, FX(1), 0, 0, ident()),
        joint(0, FX(2), 0, 0, rot_z90()),
        joint(1, FX(3), 0, 0, ident()),
    };
    sat_skeleton_asset_t s{j, 3, 0};
    sat_mat4_t inv[3]{};
    CHECK(sat_skeleton_inverse_bind(&s, inv, 2) == SAT_ERR_CAPACITY);
    CHECK(sat_skeleton_inverse_bind(&s, inv, 3) == SAT_OK);

    sat_transform3d_node_t nodes[3]{};
    uint16_t scratch[3]{};
    sat_skeleton_instance_t inst{};
    CHECK(sat_skeleton_instance_init(&inst, &s, nodes, 3, scratch, 3) == SAT_OK);
    sat_skeleton_pose_joint_t pose[3];
    CHECK(sat_skeleton_rest_pose(&s, pose, 3) == SAT_OK);
    sat_mat4_t skin[3]{};
    CHECK(sat_skeleton_skin_matrices(&inst, inv, skin, 3) == SAT_ERR_BUSY);
    CHECK(sat_skeleton_evaluate(&inst, pose, 3) == SAT_OK);
    CHECK(sat_skeleton_skin_matrices(&inst, inv, skin, 3) == SAT_OK);
    /* Rest pose: every skin matrix is the identity within rounding. */
    for (int i = 0; i < 3; ++i) {
        for (int k = 0; k < 16; ++k) {
            const int32_t want = (k % 5 == 0) ? 65536 : 0;
            CHECK(absdiff(skin[i].m[k], want) < 512);
        }
    }
    /* Posing joint 1 moves joint 1's skin matrix off identity, leaves joint 0. */
    pose[1].rotation = ident();
    CHECK(sat_skeleton_evaluate(&inst, pose, 3) == SAT_OK);
    CHECK(sat_skeleton_skin_matrices(&inst, inv, skin, 3) == SAT_OK);
    CHECK(absdiff(skin[0].m[0], 65536) < 512);
    CHECK(absdiff(skin[1].m[0], 65536) > 30000);
}

static void scaled_bind_and_skin() {
    sat_skeleton_joint_t j[2] = {
        joint(SAT_TRANSFORM3D_ROOT, FX(1), FX(-2), FX(3), rot_z90()),
        joint(0, FX(2), 0, 0, ident()),
    };
    j[0].rest_scale = {FX(2), FX(3), FX(0.5)};
    j[1].rest_scale = {FX(0.5), FX(1), FX(2)};
    sat_skeleton_asset_t s{j, 2, 0};
    sat_mat4_t inv[2]{}, skin[2]{};
    CHECK(sat_skeleton_inverse_bind(&s, inv, 2) == SAT_OK);
    sat_transform3d_node_t nodes[2]{};
    uint16_t scratch[2]{};
    sat_skeleton_instance_t inst{};
    sat_skeleton_pose_joint_t pose[2]{};
    CHECK(sat_skeleton_instance_init(&inst, &s, nodes, 2, scratch, 2) == SAT_OK);
    CHECK(sat_skeleton_rest_pose(&s, pose, 2) == SAT_OK);
    CHECK(sat_skeleton_evaluate(&inst, pose, 2) == SAT_OK);
    CHECK(sat_skeleton_skin_matrices(&inst, inv, skin, 2) == SAT_OK);
    for (int i = 0; i < 2; ++i) for (int k = 0; k < 16; ++k)
        CHECK(absdiff(skin[i].m[k], k % 5 == 0 ? 65536 : 0) < 512);
}

static void blend_poses() {
    sat_skeleton_asset_t s{kArm, 3, 0};
    sat_skeleton_pose_joint_t a[3], b[3], out[3];
    CHECK(sat_skeleton_rest_pose(&s, a, 3) == SAT_OK);
    CHECK(sat_skeleton_rest_pose(&s, b, 3) == SAT_OK);
    b[0].translation.x = FX(3);
    b[1].rotation = rot_z90();
    CHECK(sat_skeleton_pose_blend(out, a, b, 3, FX(1) / 2) == SAT_OK);
    CHECK(out[0].translation.x == FX(2));
    CHECK(absdiff(out[1].rotation.z, 12540) < 400);
    CHECK(sat_skeleton_pose_blend(out, a, b, 3, 0) == SAT_OK);
    CHECK(out[0].translation.x == FX(1) && out[1].rotation.z == 0);
    /* aliasing out == a is allowed */
    CHECK(sat_skeleton_pose_blend(a, a, b, 3, FX(1)) == SAT_OK);
    CHECK(a[0].translation.x == FX(3));
    CHECK(sat_skeleton_pose_blend(nullptr, a, b, 3, 0) == SAT_ERR_INVALID_ARG);
}

static void instances_are_independent() {
    sat_skeleton_asset_t s{kArm, 3, 0};
    sat_transform3d_node_t n1[3]{}, n2[3]{};
    uint16_t s1[3]{}, s2[3]{};
    sat_skeleton_instance_t i1{}, i2{};
    CHECK(sat_skeleton_instance_init(&i1, &s, n1, 3, s1, 3) == SAT_OK);
    CHECK(sat_skeleton_instance_init(&i2, &s, n2, 3, s2, 3) == SAT_OK);
    sat_skeleton_pose_joint_t p1[3], p2[3];
    sat_skeleton_rest_pose(&s, p1, 3);
    sat_skeleton_rest_pose(&s, p2, 3);
    p2[0].translation.x = FX(10);
    CHECK(sat_skeleton_evaluate(&i1, p1, 3) == SAT_OK);
    CHECK(sat_skeleton_evaluate(&i2, p2, 3) == SAT_OK);
    sat_mat4_t m1{}, m2{};
    sat_skeleton_joint_world(&i1, 2, &m1);
    sat_skeleton_joint_world(&i2, 2, &m2);
    CHECK(m1.m[3] == FX(6) && m2.m[3] == FX(15));
}

static void caller_owned_large_hierarchy_and_unchanged_locals() {
    constexpr uint16_t JointCount=64;
    sat_skeleton_joint_t joints[JointCount]{};
    sat_skeleton_pose_joint_t pose[JointCount]{};
    sat_transform3d_node_t nodes[JointCount]{};uint16_t scratch[JointCount]{};
    for(uint16_t j=0;j<JointCount;++j) {
        joints[j].parent=j?j-1:SAT_TRANSFORM3D_ROOT;
        joints[j].rest_translation.y=FX(1);sat_quat16_identity(&joints[j].rest_rotation);
    }
    sat_skeleton_asset_t asset{joints,JointCount,0};sat_skeleton_instance_t instance{};
    CHECK(sat_skeleton_instance_init(&instance,&asset,nodes,JointCount,scratch,JointCount)==SAT_OK);
    CHECK(sat_skeleton_rest_pose(&asset,pose,JointCount)==SAT_OK);
    CHECK(sat_skeleton_evaluate(&instance,pose,JointCount)==SAT_OK);
    sat_mat4_t before=nodes[JointCount-1].world;
    CHECK(before.m[7]==FX(JointCount));
    CHECK(sat_skeleton_evaluate(&instance,pose,JointCount)==SAT_OK);
    CHECK(std::memcmp(&before,&nodes[JointCount-1].world,sizeof(before))==0);
    pose[0].translation.x=FX(2);
    CHECK(sat_skeleton_evaluate(&instance,pose,JointCount)==SAT_OK);
    CHECK(nodes[JointCount-1].world.m[3]==FX(2) && nodes[JointCount-1].world.m[7]==FX(JointCount));
}
int main() {
    caller_owned_large_hierarchy_and_unchanged_locals();
    quat_basics();
    nlerp_cases();
    validation();
    one_bone();
    hierarchy_and_rotation();
    bind_and_skin();
    scaled_bind_and_skin();
    blend_poses();
    instances_are_independent();
    std::printf("test_skeleton3d: 10 tests passed\n");
    return 0;
}
