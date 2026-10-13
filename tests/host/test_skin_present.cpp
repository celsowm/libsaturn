#include <cstdio>
#include <cstdlib>
#include "saturn/skin3d.h"

#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (0)
#define FX(n) ((sat_fx16_t)((n) * 65536))

static sat_vec3_t kQuad[4] = {{FX(-1), FX(-1), 0}, {FX(1), FX(-1), 0}, {FX(1), FX(1), 0}, {FX(-1), FX(1), 0}};

static void views_and_lod() {
    sat_vec3_t v[8] = {};
    uint16_t idx[12] = {0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5};
    sat_mesh_t m = {v, idx, 8, 8, 3, 3};
    sat_mesh_t view = {};
    CHECK(sat_mesh_face_view(&m, {1, 2}, &view) == SAT_OK);
    CHECK(view.vertices == v && view.indices == idx + 4 && view.face_count == 2);
    CHECK(view.indices[0] == 1 && view.indices[4] == 2);
    CHECK(sat_mesh_face_view(&m, {0, 3}, &view) == SAT_OK && view.face_count == 3);
    sat_mesh_t keep = view;
    CHECK(sat_mesh_face_view(&m, {2, 2}, &view) == SAT_ERR_INVALID_ARG);
    CHECK(view.face_count == keep.face_count && view.indices == keep.indices);
    CHECK(sat_mesh_face_view(nullptr, {0, 1}, &view) == SAT_ERR_INVALID_ARG);

    const sat_face_range_t lods[3] = {{0, 800}, {800, 400}, {1200, 150}};
    uint16_t lvl = 99;
    CHECK(sat_lod_select(lods, 3, 1000, &lvl) == SAT_OK && lvl == 0);
    CHECK(sat_lod_select(lods, 3, 500, &lvl) == SAT_OK && lvl == 1);
    CHECK(sat_lod_select(lods, 3, 150, &lvl) == SAT_OK && lvl == 2);
    CHECK(sat_lod_select(lods, 3, 149, &lvl) == SAT_ERR_CAPACITY && lvl == 2); /* explicit, not silent */
    CHECK(sat_lod_select(nullptr, 3, 10, &lvl) == SAT_ERR_INVALID_ARG);
    CHECK(sat_lod_select(lods, 0, 10, &lvl) == SAT_ERR_INVALID_ARG);
}

static void ramp(double scale, int32_t ambient, const sat_vec3_t& light, uint16_t expect, uint16_t ramp_count = 5) {
    sat_vec3_t v[4];
    for (int i = 0; i < 4; ++i)
        v[i] = {(sat_fx16_t)(kQuad[i].x * scale), (sat_fx16_t)(kQuad[i].y * scale), 0};
    uint16_t idx[4] = {0, 1, 2, 3};
    sat_mesh_t m = {v, idx, 4, 4, 1, 1};
    uint16_t out = 99;
    CHECK(sat_mesh_face_ramp(&m, &light, ambient, ramp_count, &out, 1) == SAT_OK);
    if (out != expect) std::fprintf(stderr, "ramp got %u want %u\n", out, expect);
    CHECK(out == expect);
}

static void lighting() {
    /* This quad's outward normal is -Z (cross(D - A, B - A)). */
    const sat_vec3_t toward = {0, 0, FX(-1)}, away = {0, 0, FX(1)}, side = {FX(1), 0, 0};
    ramp(1, FX(1) / 2, toward, 4);        /* full light -> brightest */
    ramp(1, FX(1) / 2, away, 2);          /* facing away -> ambient only */
    ramp(1, FX(1) / 2, side, 2);          /* grazing -> ambient */
    ramp(1, 0, toward, 4);
    ramp(1, 0, away, 0);
    ramp(1, FX(1), toward, 4);            /* ambient 1.0 ignores the light */
    ramp(1, FX(1) / 2, {FX(1), 0, FX(-1)}, 3); /* 45 degrees, unnormalised light */
    ramp(100, FX(1) / 2, toward, 4);      /* 100-unit quad: no overflow, same answer */
    ramp(0.01, FX(1) / 2, toward, 4);
    ramp(1, 0, toward, 1, 2);             /* two-entry ramp */

    /* degenerate face and error paths */
    sat_vec3_t same[4] = {kQuad[0], kQuad[0], kQuad[0], kQuad[0]};
    uint16_t idx[4] = {0, 1, 2, 3}, out = 99;
    sat_mesh_t m = {same, idx, 4, 4, 1, 1};
    CHECK(sat_mesh_face_ramp(&m, &toward, FX(1) / 2, 5, &out, 1) == SAT_OK && out == 2);
    sat_vec3_t zero = {0, 0, 0};
    CHECK(sat_mesh_face_ramp(&m, &zero, FX(1) / 2, 5, &out, 1) == SAT_ERR_INVALID_ARG);
    CHECK(sat_mesh_face_ramp(&m, &toward, FX(1) / 2, 1, &out, 1) == SAT_ERR_INVALID_ARG);
    CHECK(sat_mesh_face_ramp(&m, &toward, -1, 5, &out, 1) == SAT_ERR_INVALID_ARG);
    CHECK(sat_mesh_face_ramp(&m, &toward, FX(2), 5, &out, 1) == SAT_ERR_INVALID_ARG);
    CHECK(sat_mesh_face_ramp(&m, &toward, FX(1) / 2, 5, &out, 0) == SAT_ERR_CAPACITY);
    uint16_t bad[4] = {0, 1, 2, 9};
    sat_mesh_t bm = {same, bad, 4, 4, 1, 1};
    out = 77;
    CHECK(sat_mesh_face_ramp(&bm, &toward, FX(1) / 2, 5, &out, 1) == SAT_ERR_INVALID_ARG && out == 77);
}

static void requirements_and_attachments() {
    constexpr uint16_t JointCount=2,FaceCount=7;
    sat_skeleton_joint_t joints[JointCount]{};
    joints[0].parent=SAT_TRANSFORM3D_ROOT;joints[1].parent=0;
    joints[0].rest_translation.x=FX(1);joints[1].rest_translation.y=FX(1);
    for(auto& joint:joints)sat_quat16_identity(&joint.rest_rotation);
    sat_skeleton_asset_t skeleton{joints,JointCount,0};
    sat_skin_rigid_t rigid={};sat_skin_asset_t skin{&rigid,nullptr,nullptr,1,0,JointCount,15,0};
    sat_skin_requirements_t needs{};
    CHECK(sat_skin_requirements(&skeleton,&skin,FaceCount,&needs)==SAT_OK);
    CHECK(needs.skeleton_bytes==sizeof(joints) && needs.vertex_bytes==sizeof(sat_vec3_t));
    CHECK(needs.command_upper_bound==FaceCount && needs.joint_matrix_bytes==JointCount*sizeof(sat_mat4_t));
    sat_transform3d_node_t nodes[JointCount]{};uint16_t scratch[JointCount]{};
    sat_skeleton_instance_t instance{};sat_skeleton_pose_joint_t pose[JointCount]{};
    CHECK(sat_skeleton_instance_init(&instance,&skeleton,nodes,JointCount,scratch,JointCount)==SAT_OK);
    sat_mat4_t world,local,result;
    sat_mat4_translate(&world,FX(10),0,0);sat_mat4_translate(&local,0,0,FX(1));
    CHECK(sat_skeleton_attachment(&instance,1,&world,&local,&result)==SAT_ERR_BUSY);
    CHECK(sat_skeleton_rest_pose(&skeleton,pose,JointCount)==SAT_OK);
    CHECK(sat_skeleton_evaluate(&instance,pose,JointCount)==SAT_OK);
    CHECK(sat_skeleton_attachment(&instance,1,&world,&local,&result)==SAT_OK);
    CHECK(result.m[3]==FX(11) && result.m[7]==FX(1) && result.m[11]==FX(1));
    CHECK(sat_skeleton_attachment(&instance,JointCount,&world,&local,&result)==SAT_ERR_INVALID_ARG);
}
int main() {
    requirements_and_attachments();
    views_and_lod();
    lighting();
    std::printf("test_skin_present: 3 tests passed\n");
    return 0;
}
