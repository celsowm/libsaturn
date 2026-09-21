#include <cassert>
#include <cstdio>
#include "saturn/scene.h"

static uint16_t calls;
static sat_fx16_t pos_x;
static const sat_mesh_t* submitted_mesh;
static uint8_t submitted_slot;
static uint16_t submitted_pass;
static sat_projected_vertex_t* submitted_screen;
static sat_vec3_t* submitted_world;

extern "C" sat_result_t sat_scene_submit_instance(
    sat_scene_t* scene, const sat_scene3d_instance_t* instance,
    uint8_t slot, sat_projected_vertex_t* screen, sat_vec3_t* world) {
    assert(scene && scene->active && instance && instance->world);
    ++calls;
    pos_x = instance->world->m[3];
    submitted_mesh = instance->mesh;
    submitted_slot = slot;
    submitted_pass = instance->pass;
    submitted_screen = screen;
    submitted_world = world;
    return SAT_OK;
}

int main() {
    sat_transform3d_node_t nodes[3]{};
    sat_transform3d_world_t hierarchy{};
    uint16_t scratch[3]{};
    uint16_t root, child;
    assert(sat_transform3d_world_init(&hierarchy, nodes, 3) == SAT_OK);
    assert(sat_transform3d_create(&hierarchy, &root) == SAT_OK);
    assert(sat_transform3d_create(&hierarchy, &child) == SAT_OK);
    assert(sat_transform3d_set_parent(&hierarchy, child, root) == SAT_OK);
    sat_model_transform3d_t t{};
    sat_model_transform3d_identity(&t);
    t.position.x = sat_fx16_from_int(3);
    assert(sat_transform3d_set_local(&hierarchy, root, &t) == SAT_OK);
    t.position.x = sat_fx16_from_int(4);
    assert(sat_transform3d_set_local(&hierarchy, child, &t) == SAT_OK);
    sat_scene_t scene{};
    scene.active = 1;
    sat_mesh_t mesh{};
    sat_mat4_t original{};
    assert(sat_mat4_identity(&original) == SAT_OK);
    sat_scene3d_instance_t prototype{};
    prototype.mesh = &mesh;
    prototype.world = &original;
    prototype.pass = 11;
    sat_projected_vertex_t screen[1]{};
    sat_vec3_t world_scratch[1]{};
    assert(sat_scene_submit_transform_instance(
        &scene, &hierarchy, child, &prototype, SAT_SCENE3D_SLOT_INHERIT,
        screen, world_scratch) == SAT_ERR_BUSY);
    assert(calls == 0);
    assert(sat_transform3d_evaluate(&hierarchy, scratch, 3) == SAT_OK);
    assert(sat_scene_submit_transform_instance(
        &scene, &hierarchy, child, &prototype, 7, screen, world_scratch) == SAT_OK);
    assert(calls == 1 && pos_x == sat_fx16_from_int(7));
    assert(submitted_mesh == &mesh && submitted_slot == 7 && submitted_pass == 11);
    assert(submitted_screen == screen && submitted_world == world_scratch);
    assert(prototype.world == &original && original.m[3] == 0);
    assert(sat_scene_submit_transform_instance(
        &scene, &hierarchy, 99, &prototype, 7, screen, world_scratch)
        == SAT_ERR_INVALID_ARG);
    assert(calls == 1);
    scene.active = 0;
    assert(sat_scene_submit_transform_instance(
        &scene, &hierarchy, child, &prototype, 7, screen, world_scratch)
        == SAT_ERR_INVALID_ARG);
    scene.active = 1;
    t.position.x = sat_fx16_from_int(5);
    assert(sat_transform3d_set_local(&hierarchy, root, &t) == SAT_OK);
    assert(sat_scene_submit_transform_instance(
        &scene, &hierarchy, child, &prototype, 7, screen, world_scratch)
        == SAT_ERR_BUSY);
    assert(sat_transform3d_evaluate(&hierarchy, scratch, 3) == SAT_OK);
    assert(sat_scene_submit_transform_instance(
        &scene, &hierarchy, child, &prototype, 7, screen, world_scratch) == SAT_OK);
    assert(calls == 2 && pos_x == sat_fx16_from_int(9));
    std::puts("test_scene_transform3d: passed");
    return 0;
}
