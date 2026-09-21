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

int main() {
    hierarchy_and_cache();
    reparent_later_created_parent();
    std::puts("test_transform3d: 2 tests passed");
    return 0;
}
