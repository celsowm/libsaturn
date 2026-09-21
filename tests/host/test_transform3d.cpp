#include <cstdio>
#include <cstdlib>
#include "saturn/transform3d.h"

#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d: %s\\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (0)
#define FX(n) ((sat_fx16_t)((n) * 65536))

static sat_mat4_t matrix(const sat_transform3d_world_t* world, uint16_t id) {
    sat_mat4_t out{};
    CHECK(sat_transform3d_get_world(world, id, &out) == SAT_OK);
    return out;
}

static void hierarchy_and_cache() {
    sat_transform3d_node_t nodes[4]{};
    sat_transform3d_world_t world{};
    uint16_t scratch[4]{};
    uint16_t root, child, grandchild, other;
    CHECK(sat_transform3d_world_init(&world, nodes, 4) == SAT_OK);
    CHECK(sat_transform3d_create(&world, &root) == SAT_OK);
    CHECK(sat_transform3d_create(&world, &child) == SAT_OK);
    CHECK(sat_transform3d_create(&world, &grandchild) == SAT_OK);
    CHECK(sat_transform3d_create(&world, &other) == SAT_OK);
    CHECK(sat_transform3d_create(&world, &other) == SAT_ERR_CAPACITY);
    CHECK(sat_transform3d_set_parent(&world, child, root) == SAT_OK);
    CHECK(sat_transform3d_set_parent(&world, grandchild, child) == SAT_OK);
    sat_model_transform3d_t t{};
    sat_model_transform3d_identity(&t);
    t.position.x = FX(2);
    CHECK(sat_transform3d_set_local(&world, root, &t) == SAT_OK);
    t.position.x = FX(3);
    CHECK(sat_transform3d_set_local(&world, child, &t) == SAT_OK);
    t.position.x = FX(4);
    CHECK(sat_transform3d_set_local(&world, grandchild, &t) == SAT_OK);
    sat_mat4_t out{};
    CHECK(sat_transform3d_get_world(&world, grandchild, &out) == SAT_ERR_BUSY);
    CHECK(sat_transform3d_evaluate(&world, scratch, 2) == SAT_ERR_CAPACITY);
    CHECK(sat_transform3d_evaluate(&world, scratch, 4) == SAT_OK);
    CHECK(matrix(&world, grandchild).m[3] == FX(9));
    CHECK(matrix(&world, other).m[3] == 0);
    CHECK(sat_transform3d_evaluate(&world, scratch, 4) == SAT_OK);
    t.position.x = FX(5);
    CHECK(sat_transform3d_set_local(&world, root, &t) == SAT_OK);
    CHECK(sat_transform3d_evaluate(&world, scratch, 4) == SAT_OK);
    CHECK(matrix(&world, grandchild).m[3] == FX(12));
    CHECK(matrix(&world, other).m[3] == 0);
    CHECK(sat_transform3d_set_parent(&world, root, grandchild) == SAT_ERR_INVALID_ARG);
    CHECK(sat_transform3d_set_parent(&world, child, child) == SAT_ERR_INVALID_ARG);
    CHECK(sat_transform3d_set_parent(&world, child, 99) == SAT_ERR_INVALID_ARG);
    CHECK(sat_transform3d_set_parent(&world, child, SAT_TRANSFORM3D_ROOT) == SAT_OK);
    CHECK(sat_transform3d_evaluate(&world, scratch, 4) == SAT_OK);
    CHECK(matrix(&world, grandchild).m[3] == FX(7));
    sat_transform3d_world_reset(&world);
    CHECK(world.count == 0);
    CHECK(sat_transform3d_create(&world, &root) == SAT_OK && root == 0);
}

static void reparent_later_created_parent() {
    sat_transform3d_node_t nodes[3]{};
    sat_transform3d_world_t world{};
    uint16_t scratch[3]{};
    uint16_t child, parent, grandparent;
    CHECK(sat_transform3d_world_init(&world, nodes, 3) == SAT_OK);
    CHECK(sat_transform3d_create(&world, &child) == SAT_OK);
    CHECK(sat_transform3d_create(&world, &parent) == SAT_OK);
    CHECK(sat_transform3d_create(&world, &grandparent) == SAT_OK);
    CHECK(sat_transform3d_set_parent(&world, child, parent) == SAT_OK);
    CHECK(sat_transform3d_set_parent(&world, parent, grandparent) == SAT_OK);
    sat_model_transform3d_t t{};
    sat_model_transform3d_identity(&t);
    t.position.y = FX(1);
    CHECK(sat_transform3d_set_local(&world, child, &t) == SAT_OK);
    t.position.y = FX(2);
    CHECK(sat_transform3d_set_local(&world, parent, &t) == SAT_OK);
    t.position.y = FX(3);
    CHECK(sat_transform3d_set_local(&world, grandparent, &t) == SAT_OK);
    CHECK(sat_transform3d_evaluate(&world, scratch, 3) == SAT_OK);
    CHECK(matrix(&world, child).m[7] == FX(6));
    CHECK(sat_transform3d_set_parent(&world, grandparent, child) == SAT_ERR_INVALID_ARG);
}

static void exact_local_matrix_and_trs_switch() {
    sat_transform3d_node_t nodes[3]{};
    sat_transform3d_world_t w{};
    uint16_t scratch[3]{};
    uint16_t parent=0,child=0,grandchild=0;
    CHECK(sat_transform3d_world_init(&w,nodes,3)==SAT_OK);
    CHECK(sat_transform3d_create(&w,&parent)==SAT_OK);
    CHECK(sat_transform3d_create(&w,&child)==SAT_OK);
    CHECK(sat_transform3d_create(&w,&grandchild)==SAT_OK);
    CHECK(sat_transform3d_set_parent(&w,child,parent)==SAT_OK);
    CHECK(sat_transform3d_set_parent(&w,grandchild,child)==SAT_OK);
    sat_model_transform3d_t local{};
    sat_model_transform3d_identity(&local);
    local.position.x=FX(1);
    CHECK(sat_transform3d_set_local(&w,grandchild,&local)==SAT_OK);
    sat_mat4_t rot{};
    CHECK(sat_mat4_rotate_z(&rot,FX(90))==SAT_OK);
    rot.m[3]=FX(2);
    CHECK(sat_transform3d_set_local_matrix(&w,parent,&rot)==SAT_OK);
    CHECK(sat_transform3d_set_local_matrix(&w,child,&rot)==SAT_OK);
    CHECK(sat_transform3d_get_world(&w,grandchild,&rot)==SAT_ERR_BUSY);
    CHECK(sat_transform3d_evaluate(&w,scratch,3)==SAT_OK);
    sat_mat4_t p=matrix(&w,parent), c=matrix(&w,child);
    CHECK(p.m[3]==FX(2));
    CHECK(c.m[3]>FX(1) && c.m[3]<FX(3));
    CHECK(matrix(&w,grandchild).m[3]<c.m[3]);
    const sat_mat4_t old=matrix(&w,child);
    sat_mat4_t invalid=rot;
    invalid.m[15]=0;
    CHECK(sat_transform3d_set_local_matrix(&w,child,&invalid)==SAT_ERR_INVALID_ARG);
    CHECK(!w.pending && matrix(&w,child).m[3]==old.m[3]);
    local.position.x=FX(5);
    CHECK(sat_transform3d_set_local(&w,child,&local)==SAT_OK);
    CHECK(!w.nodes[child].use_local_matrix);
    CHECK(sat_transform3d_evaluate(&w,scratch,3)==SAT_OK);
    CHECK(matrix(&w,child).m[7]!=old.m[7]);
    CHECK(sat_transform3d_set_local_matrix(&w,12,&rot)==SAT_ERR_INVALID_ARG);
    sat_transform3d_world_reset(&w);
    CHECK(sat_transform3d_create(&w,&parent)==SAT_OK);
    CHECK(w.nodes[parent].use_local_matrix==0);
}
int main() {
    hierarchy_and_cache();
    reparent_later_created_parent();
    exact_local_matrix_and_trs_switch();
    std::puts("test_transform3d: 3 tests passed");
    return 0;
}
