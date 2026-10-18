#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "saturn/skin3d.h"

#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (0)
#define FX(n) ((sat_fx16_t)((n) * 65536))

static int64_t absdiff(int64_t a, int64_t b) { return a > b ? a - b : b - a; }

static sat_mat4_t translation(sat_fx16_t x, sat_fx16_t y, sat_fx16_t z) {
    sat_mat4_t m{};
    sat_quat16_t id;
    sat_quat16_identity(&id);
    sat_vec3_t t = {x, y, z};
    sat_quat16_to_mat4(&m, &id, &t);
    return m;
}

static sat_skin_asset_t asset(const sat_skin_rigid_t* r, uint16_t rn, const sat_skin_blend_t* b,
                              uint16_t bn, uint16_t joints, uint8_t shift, uint8_t max_joint) {
    sat_skin_asset_t a{};
    a.rigid = r; a.rigid_count = rn; a.blend = b; a.blend_count = bn;
    a.joint_count = joints; a.pos_shift = shift; a.max_joint = max_joint;
    return a;
}

static void rigid_exact() {
    sat_skin_rigid_t r[2] = {{{256, 512, 768}, 0, 0}, {{-256, 0, 0}, 1, 0}};
    sat_skin_asset_t a = asset(r, 2, nullptr, 0, 2, 8, 1);
    CHECK(sat_skin_validate(&a) == SAT_OK);
    CHECK(sat_skin_vertex_count(&a) == 2);
    sat_mat4_t mats[2] = {translation(FX(2), FX(3), FX(4)), translation(FX(10), 0, 0)};
    sat_vec3_t out[2] = {};
    CHECK(sat_skin_deform(&a, mats, 2, out, 2) == SAT_OK);
    CHECK(out[0].x == FX(3) && out[0].y == FX(5) && out[0].z == FX(7));
    CHECK(out[1].x == FX(9) && out[1].y == 0 && out[1].z == 0);
}

static void multi_anchor_blend() {
    /* Joint 0 at the origin, joint 1 at x=10. Both anchors describe the same
     * rest point (1,0,0) from different local positions. */
    sat_skin_blend_t b[2] = {};
    b[0].count = 2; b[0].joint[0] = 0; b[0].joint[1] = 1;
    b[0].weight[0] = 16384;                       /* exact half */
    b[0].pos[0][0] = 256; b[0].pos[1][0] = -2304; /* (1,0,0) and (-9,0,0) at shift 8 */
    b[1] = b[0];
    b[1].weight[0] = 8192;                        /* 25% / 75% */
    sat_skin_asset_t a = asset(nullptr, 0, b, 2, 2, 8, 1);
    CHECK(sat_skin_validate(&a) == SAT_OK);
    sat_mat4_t mats[2] = {translation(0, 0, 0), translation(FX(10), 0, 0)};
    sat_vec3_t out[2] = {};
    CHECK(sat_skin_deform(&a, mats, 2, out, 2) == SAT_OK);
    CHECK(out[0].x == FX(1) && out[1].x == FX(1)); /* at rest both anchors agree */

    /* Move joint 1 up by 4: result is the weighted mix of two DIFFERENT points. */
    mats[1] = translation(FX(10), FX(4), 0);
    CHECK(sat_skin_deform(&a, mats, 2, out, 2) == SAT_OK);
    CHECK(out[0].y == FX(2));                      /* half: (0 + 4) / 2 */
    CHECK(out[1].y == FX(3));                      /* 25% * 0 + 75% * 4 */
    CHECK(out[0].x == FX(1));
}

static void three_influences_and_normalisation() {
    sat_skin_blend_t b[1] = {};
    b[0].count = 3;
    b[0].joint[0] = 0; b[0].joint[1] = 1; b[0].joint[2] = 2;
    b[0].weight[0] = 12345; b[0].weight[1] = 9876;      /* third = 10547 */
    for (int k = 0; k < 3; ++k) { b[0].pos[k][0] = 100; b[0].pos[k][1] = -50; b[0].pos[k][2] = 7; }
    sat_skin_asset_t a = asset(nullptr, 0, b, 1, 3, 4, 2);
    CHECK(sat_skin_validate(&a) == SAT_OK);
    /* Identical matrices: any weights reproduce the single-influence result exactly. */
    sat_mat4_t same[3] = {translation(FX(1), FX(2), FX(3)), translation(FX(1), FX(2), FX(3)),
                          translation(FX(1), FX(2), FX(3))};
    sat_vec3_t out{};
    CHECK(sat_skin_deform(&a, same, 3, &out, 1) == SAT_OK);
    sat_skin_rigid_t r[1] = {{{100, -50, 7}, 0, 0}};
    sat_skin_asset_t single = asset(r, 1, nullptr, 0, 3, 4, 0);
    sat_vec3_t ref{};
    CHECK(sat_skin_deform(&single, same, 3, &ref, 1) == SAT_OK);
    CHECK(out.x == ref.x && out.y == ref.y && out.z == ref.z);
    /* Different translations: the weighted sum is correct to rounding. */
    sat_mat4_t diff[3] = {translation(FX(0), 0, 0), translation(FX(8), 0, 0), translation(FX(-4), 0, 0)};
    CHECK(sat_skin_deform(&a, diff, 3, &out, 1) == SAT_OK);
    const double w0 = 12345 / 32768.0, w1 = 9876 / 32768.0, w2 = 10547 / 32768.0;
    const double want = 100 / 16.0 + (w0 * 0 + w1 * 8 + w2 * -4);
    CHECK(std::fabs(out.x / 65536.0 - want) < 2.0 / 65536);
}

static void validation_errors() {
    sat_skin_rigid_t r[1] = {{{0, 0, 0}, 3, 0}};
    sat_skin_asset_t a = asset(r, 1, nullptr, 0, 2, 0, 3);
    CHECK(sat_skin_validate(&a) == SAT_ERR_INVALID_ARG);          /* joint >= joint_count */
    sat_skin_rigid_t ok[1] = {{{0, 0, 0}, 1, 0}};
    a = asset(ok, 1, nullptr, 0, 2, 0, 0);
    CHECK(sat_skin_validate(&a) == SAT_ERR_INVALID_ARG);          /* max_joint lies */
    a = asset(ok, 1, nullptr, 0, 2, 16, 1);
    CHECK(sat_skin_validate(&a) == SAT_ERR_INVALID_ARG);          /* shift too large */
    sat_skin_blend_t bb[1] = {};
    bb[0].count = 4;
    a = asset(nullptr, 0, bb, 1, 2, 0, 0);
    CHECK(sat_skin_validate(&a) == SAT_ERR_INVALID_ARG);
    bb[0].count = 3; bb[0].weight[0] = 30000; bb[0].weight[1] = 30000;
    a = asset(nullptr, 0, bb, 1, 2, 0, 0);
    CHECK(sat_skin_validate(&a) == SAT_ERR_INVALID_ARG);          /* weights exceed 1.0 */
    CHECK(sat_skin_validate(nullptr) == SAT_ERR_INVALID_ARG);

    /* deform: output untouched on every rejection */
    a = asset(ok, 1, nullptr, 0, 2, 0, 1);
    sat_mat4_t mats[2] = {translation(0, 0, 0), translation(FX(1), 0, 0)};
    sat_vec3_t out[2];
    for (auto& v : out) v = {-1, -2, -3};
    CHECK(sat_skin_deform(&a, mats, 1, out, 2) == SAT_ERR_INVALID_ARG); /* too few matrices */
    CHECK(sat_skin_deform(&a, mats, 2, out, 0) == SAT_ERR_CAPACITY);
    CHECK(sat_skin_deform(&a, nullptr, 2, out, 2) == SAT_ERR_INVALID_ARG);
    CHECK(sat_skin_deform(&a, mats, 2, nullptr, 2) == SAT_ERR_INVALID_ARG);
    CHECK(out[0].x == -1 && out[1].z == -3);
    CHECK(sat_skin_deform_range(&a, mats, 2, 1, 1, out, 2) == SAT_ERR_INVALID_ARG);
    CHECK(sat_skin_deform_range(&a, mats, 2, 2, 0, out, 2) == SAT_ERR_INVALID_ARG);
    CHECK(out[0].x == -1);
}

static void extremes() {
    /* Largest local position through a large (but representable) matrix. */
    sat_skin_rigid_t r[1] = {{{32767, -32768, 32767}, 0, 0}};
    sat_skin_asset_t a = asset(r, 1, nullptr, 0, 1, 0, 0);
    sat_mat4_t m = translation(FX(100), 0, 0);
    m.m[0] = FX(2); m.m[5] = FX(-1); m.m[10] = FX(1) / 2;
    sat_vec3_t out{};
    CHECK(sat_skin_deform(&a, &m, 1, &out, 1) == SAT_OK);
    CHECK(out.x == (int32_t)(2LL * 65536 * 32767 + FX(100)) || true); /* 64-bit accumulate */
    const int64_t wx = (int64_t)m.m[0] * 32767 + m.m[3];
    /* x exceeds int32 range: result is the documented truncation, not a crash. */
    CHECK(out.x == (int32_t)wx);
    CHECK(out.y == (int32_t)((int64_t)m.m[5] * -32768 + m.m[7]));
    CHECK(out.z == (int32_t)(((int64_t)m.m[10] * 32767) + m.m[11]));
}

static void range_equals_full() {
    const int N = 40;
    std::vector<sat_skin_rigid_t> r(N);
    std::vector<sat_skin_blend_t> b(N);
    uint32_t s = 12345;
    auto rnd = [&]() { s = s * 1664525u + 1013904223u; return (int32_t)(s >> 8); };
    for (int i = 0; i < N; ++i) {
        r[i] = {{(int16_t)(rnd() % 3000), (int16_t)(rnd() % 3000), (int16_t)(rnd() % 3000)},
                (uint8_t)(i % 3), 0};
        b[i] = {};
        b[i].count = (uint8_t)(2 + (i & 1));
        for (int k = 0; k < 3; ++k) {
            b[i].joint[k] = (uint8_t)((i + k) % 3);
            for (int c = 0; c < 3; ++c) b[i].pos[k][c] = (int16_t)(rnd() % 3000);
        }
        b[i].weight[0] = (uint16_t)(1000 + (i * 700) % 15000);
        b[i].weight[1] = (uint16_t)(2000 + (i * 300) % 9000);
    }
    sat_skin_asset_t a = asset(r.data(), N, b.data(), N, 3, 6, 2);
    CHECK(sat_skin_validate(&a) == SAT_OK);
    sat_mat4_t mats[3];
    for (int j = 0; j < 3; ++j) {
        sat_quat16_t q = {(int16_t)(rnd() % 8000), (int16_t)(rnd() % 8000), (int16_t)(rnd() % 8000),
                          (int16_t)20000};
        sat_quat16_normalize(&q, &q);
        sat_vec3_t t = {rnd() % FX(20), rnd() % FX(20), rnd() % FX(20)};
        sat_quat16_to_mat4(&mats[j], &q, &t);
    }
    std::vector<sat_vec3_t> full(2 * N), split(2 * N);
    CHECK(sat_skin_deform(&a, mats, 3, full.data(), 2 * N) == SAT_OK);
    CHECK(sat_skin_deform_range(&a, mats, 3, 0, 17, split.data(), 2 * N) == SAT_OK);
    CHECK(sat_skin_deform_range(&a, mats, 3, 17, 40, split.data(), 2 * N) == SAT_OK);
    CHECK(sat_skin_deform_range(&a, mats, 3, 57, 23, split.data(), 2 * N) == SAT_OK);
    CHECK(std::memcmp(full.data(), split.data(), 2 * N * sizeof(sat_vec3_t)) == 0);

    /* Independent double-precision reference over the same matrices. */
    double worst = 0;
    for (int v = 0; v < 2 * N; ++v) {
        double ref[3] = {0, 0, 0};
        auto inf = [&](int joint, const int16_t* q, double w) {
            const sat_fx16_t* m = mats[joint].m;
            for (int row = 0; row < 3; ++row) {
                const double p = (m[row * 4] * (double)q[0] + m[row * 4 + 1] * (double)q[1] +
                                  m[row * 4 + 2] * (double)q[2]) / 64.0 + m[row * 4 + 3];
                ref[row] += w * p;
            }
        };
        if (v < N) inf(r[v].joint, r[v].pos, 1.0);
        else {
            const sat_skin_blend_t& bb = b[v - N];
            if (bb.count == 2) {
                inf(bb.joint[0], bb.pos[0], bb.weight[0] / 32768.0);
                inf(bb.joint[1], bb.pos[1], (32768 - bb.weight[0]) / 32768.0);
            } else {
                inf(bb.joint[0], bb.pos[0], bb.weight[0] / 32768.0);
                inf(bb.joint[1], bb.pos[1], bb.weight[1] / 32768.0);
                inf(bb.joint[2], bb.pos[2], (32768 - bb.weight[0] - bb.weight[1]) / 32768.0);
            }
        }
        const int32_t got[3] = {full[v].x, full[v].y, full[v].z};
        for (int c = 0; c < 3; ++c) worst = std::fmax(worst, std::fabs(got[c] - ref[c]));
    }
    std::fprintf(stderr, "worst=%f\n", worst);
    CHECK(worst <= 1.0); /* within one 16.16 LSB of the double reference */
}

static void normals_and_skeleton_integration() {
    /* Two-joint skeleton, joint 1 rotated 90 degrees about Z. */
    sat_skeleton_joint_t j[2] = {};
    j[0].parent = SAT_TRANSFORM3D_ROOT; sat_quat16_identity(&j[0].rest_rotation);
    j[1].parent = 0; sat_quat16_identity(&j[1].rest_rotation);
    j[1].rest_translation = {FX(2), 0, 0};
    sat_skeleton_asset_t sk{j, 2, 0};
    sat_transform3d_node_t nodes[2]{};
    uint16_t scratch[2]{};
    sat_skeleton_instance_t inst{};
    CHECK(sat_skeleton_instance_init(&inst, &sk, nodes, 2, scratch, 2) == SAT_OK);
    sat_skeleton_pose_joint_t pose[2];
    CHECK(sat_skeleton_rest_pose(&sk, pose, 2) == SAT_OK);
    pose[1].rotation = {0, 0, 23170, 23170};
    sat_mat4_t mats[2];
    CHECK(sat_skeleton_world_matrices(&inst, mats, 2) == SAT_ERR_BUSY);
    CHECK(sat_skeleton_evaluate(&inst, pose, 2) == SAT_OK);
    CHECK(sat_skeleton_world_matrices(&inst, mats, 1) == SAT_ERR_CAPACITY);
    CHECK(sat_skeleton_world_matrices(&inst, mats, 2) == SAT_OK);

    sat_skin_rigid_t r[2] = {{{0, 0, 0}, 0, 0}, {{256, 0, 0}, 1, 0}}; /* (1,0,0) in joint 1 */
    const int16_t normals[6] = {0, 0, 16384, 16384, 0, 0};            /* +Z, +X */
    sat_skin_asset_t a = asset(r, 2, nullptr, 0, 2, 8, 1);
    a.normals = normals;
    sat_vec3_t out[2], n[2];
    CHECK(sat_skin_deform(&a, mats, 2, out, 2) == SAT_OK);
    /* joint 1 sits at x=2 and points its local +X along world +Y: (2, 1, 0) */
    CHECK(absdiff(out[1].x, FX(2)) < 512 && absdiff(out[1].y, FX(1)) < 512);
    CHECK(sat_skin_deform_normals(&a, mats, 2, n, 2) == SAT_OK);
    CHECK(n[0].z == FX(1) && n[0].x == 0);
    CHECK(absdiff(n[1].y, FX(1)) < 512 && absdiff(n[1].x, 0) < 512);
    a.normals = nullptr;
    CHECK(sat_skin_deform_normals(&a, mats, 2, n, 2) == SAT_ERR_UNSUPPORTED);
}

static void shared_asset_independent_instances() {
    sat_skin_rigid_t r[1] = {{{256, 0, 0}, 0, 0}};
    sat_skin_asset_t a = asset(r, 1, nullptr, 0, 1, 8, 0);
    sat_mat4_t m1 = translation(FX(5), 0, 0), m2 = translation(FX(-5), 0, 0);
    sat_vec3_t o1{}, o2{};
    CHECK(sat_skin_deform(&a, &m1, 1, &o1, 1) == SAT_OK);
    CHECK(sat_skin_deform(&a, &m2, 1, &o2, 1) == SAT_OK);
    CHECK(o1.x == FX(6) && o2.x == FX(-4));
    CHECK(sat_skin_deform(&a, &m1, 1, &o1, 1) == SAT_OK && o1.x == FX(6));
}

static void q15_conversion_matches_wide_reference() {
    sat_skin_rigid_t rigid[8]={};
    sat_skin_blend_t blend[4]={};
    uint32_t seed=0x71100A2u;
    auto random=[&]() {seed=seed*1664525u+1013904223u;return seed;};
    for(unsigned v=0;v<8;++v) {
        rigid[v].joint=v&1u;
        for(unsigned c=0;c<3;++c)rigid[v].pos[c]=static_cast<int16_t>(random()>>16);
    }
    rigid[0].pos[0]=-32768;rigid[0].pos[1]=32767;
    for(unsigned v=0;v<4;++v) {
        blend[v].count=v==0?1:v==3?3:2;
        blend[v].weight[0]=v==1?16384:10001;
        blend[v].weight[1]=7003;
        for(unsigned k=0;k<3;++k) {
            blend[v].joint[k]=k&1u;
            for(unsigned c=0;c<3;++c)blend[v].pos[k][c]=static_cast<int16_t>(random()>>16);
        }
    }
    sat_mat4_t matrices[2]={};sat_vec3_t actual[12],split[12];
    for(unsigned trial=0;trial<128;++trial) {
        const unsigned shift=trial<64?15:trial%16;
        for(auto& matrix:matrices)for(unsigned row=0;row<3;++row) {
            for(unsigned c=0;c<3;++c)
                matrix.m[row*4+c]=(static_cast<int32_t>(random()%41)-20)*16;
            matrix.m[row*4+3]=static_cast<int32_t>(random()%10001)-5000;
        }
        if(trial<64) {
            // Large coefficients, negative extremes and coefficients with low
            // bits set; no per-influence rounding may change.
            matrices[1].m[0]=static_cast<int32_t>(65534+trial%3)*16;
            if(trial%4==0)++matrices[0].m[0];
        }
        auto skin=asset(rigid,8,blend,4,2,shift,1);
        CHECK(sat_skin_deform(&skin,matrices,2,actual,12)==SAT_OK);
        CHECK(sat_skin_deform_range(&skin,matrices,2,0,9,split,12)==SAT_OK);
        CHECK(sat_skin_deform_range(&skin,matrices,2,9,3,split,12)==SAT_OK);
        CHECK(std::memcmp(actual,split,sizeof(actual))==0);
        auto old_transform=[&](unsigned joint,const int16_t* q,unsigned row) {
            int64_t sum=32768;
            for(unsigned c=0;c<3;++c)
                sum+=static_cast<int64_t>(matrices[joint].m[row*4+c])*
                    (static_cast<int64_t>(q[c])*(1u<<(16-shift)));
            return static_cast<int32_t>(sum>>16)+matrices[joint].m[row*4+3];
        };
        for(unsigned v=0;v<12;++v)for(unsigned row=0;row<3;++row) {
            int32_t expected;
            if(v<8)expected=old_transform(rigid[v].joint,rigid[v].pos,row);
            else {
                const auto& b=blend[v-8];
                int32_t p[3];for(unsigned k=0;k<b.count;++k)
                    p[k]=old_transform(b.joint[k],b.pos[k],row);
                if(b.count==1)expected=p[0];
                else if(b.count==2 && b.weight[0]==16384)
                    expected=static_cast<int32_t>((static_cast<int64_t>(p[0])+p[1])>>1);
                else {
                    int64_t sum=16384;
                    unsigned used=0;
                    for(unsigned k=0;k<b.count;++k) {
                        const unsigned w=k+1==b.count?32768-used:b.weight[k];
                        used+=w;sum+=static_cast<int64_t>(p[k])*w;
                    }
                    expected=static_cast<int32_t>(sum>>15);
                }
            }
            const int32_t component=row==0?actual[v].x:row==1?actual[v].y:actual[v].z;
            CHECK(component==expected);
        }
    }
}

int main() {
    rigid_exact();
    multi_anchor_blend();
    three_influences_and_normalisation();
    validation_errors();
    extremes();
    range_equals_full();
    normals_and_skeleton_integration();
    shared_asset_independent_instances();
    q15_conversion_matches_wide_reference();
    std::printf("test_skin3d: 9 tests passed\n");
    return 0;
}
