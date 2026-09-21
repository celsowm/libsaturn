#include <cstdio>
#include <cstdlib>
#include "saturn/physics3_transform.h"

#define FX(n) ((sat_fx16_t)((n)*65536))
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %s:%d: %s\\n",__FILE__,__LINE__,#x);std::exit(1);}}while(0)

static void sync_root_and_inherit_quaternion() {
    const sat_vec3_t zero{0,0,0};
    sat_physics3_actor_t actors[1]{};
    sat_physics3_world_t physics{};
    CHECK(sat_physics3_world_init(&physics,actors,1,zero,8,2)==SAT_OK);
    const sat_sphere_t sphere{{FX(3),FX(4),FX(5)},FX(1)};
    const sat_vec3_t spin{0,0,-FX(1)/2};
    uint16_t sphere_id=777;
    CHECK(sat_physics3_add_sphere(&physics,&sphere,&zero,nullptr,
                                 &sphere_id)==SAT_ERR_INVALID_ARG);
    const sat_physics3_material_t material{SAT_FX16_ONE,0};
    CHECK(sat_physics3_add_sphere(&physics,&sphere,&zero,&material,
                                 &sphere_id)==SAT_OK);
    CHECK(sat_physics3_set_rolling(&physics,sphere_id,1)==SAT_OK);
    CHECK(sat_physics3_set_angular_velocity(
        &physics,sphere_id,&spin)==SAT_OK);
    CHECK(sat_physics3_world_step(&physics)==SAT_OK);
    sat_mat4_t pose{};
    CHECK(sat_physics3_sphere_model_matrix(&physics,sphere_id,&pose)==SAT_OK);
    CHECK(pose.m[4]<0);

    sat_transform3d_node_t nodes[3]{};
    sat_transform3d_world_t transforms{};
    uint16_t scratch[3]{};
    uint16_t root=777,child=777,parent=777;
    CHECK(sat_transform3d_world_init(&transforms,nodes,3)==SAT_OK);
    CHECK(sat_transform3d_create(&transforms,&root)==SAT_OK);
    CHECK(sat_transform3d_create(&transforms,&child)==SAT_OK);
    CHECK(sat_transform3d_create(&transforms,&parent)==SAT_OK);
    CHECK(sat_transform3d_set_parent(&transforms,child,root)==SAT_OK);
    sat_model_transform3d_t child_local{};
    sat_model_transform3d_identity(&child_local);
    child_local.position.x=FX(1);
    CHECK(sat_transform3d_set_local(&transforms,child,&child_local)==SAT_OK);
    CHECK(sat_physics3_sync_sphere_transform(
        &physics,sphere_id,&transforms,root)==SAT_OK);
    CHECK(transforms.nodes[root].use_local_matrix);
    sat_mat4_t cached{};
    CHECK(sat_transform3d_get_world(&transforms,root,&cached)==SAT_ERR_BUSY);
    CHECK(sat_transform3d_evaluate(&transforms,scratch,3)==SAT_OK);
    CHECK(sat_transform3d_get_world(&transforms,root,&cached)==SAT_OK);
    for(int i=0;i<16;++i)CHECK(cached.m[i]==pose.m[i]);
    sat_mat4_t child_pose{};
    CHECK(sat_transform3d_get_world(&transforms,child,&child_pose)==SAT_OK);
    CHECK(child_pose.m[3]==pose.m[3]+pose.m[0]);
    CHECK(child_pose.m[7]==pose.m[7]+pose.m[4]);
    CHECK(child_pose.m[11]==pose.m[11]+pose.m[8]);

    const sat_vec3_t velocity{FX(1)/4,0,0};
    CHECK(sat_physics3_set_velocity(&physics,sphere_id,&velocity)==SAT_OK);
    CHECK(sat_physics3_world_step(&physics)==SAT_OK);
    CHECK(sat_physics3_sync_sphere_transform(
        &physics,sphere_id,&transforms,root)==SAT_OK);
    CHECK(sat_transform3d_evaluate(&transforms,scratch,3)==SAT_OK);
    CHECK(sat_transform3d_get_world(&transforms,root,&cached)==SAT_OK);
    CHECK(cached.m[3]>pose.m[3]);

    CHECK(sat_transform3d_set_parent(&transforms,root,parent)==SAT_OK);
    CHECK(sat_transform3d_evaluate(&transforms,scratch,3)==SAT_OK);
    const sat_mat4_t before=transforms.nodes[root].world;
    CHECK(sat_physics3_sync_sphere_transform(
        &physics,sphere_id,&transforms,root)==SAT_ERR_INVALID_ARG);
    CHECK(!transforms.pending && transforms.nodes[root].world.m[3]==before.m[3]);
    CHECK(sat_physics3_sync_sphere_transform(
        &physics,99,&transforms,parent)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_sync_sphere_transform(
        &physics,sphere_id,&transforms,99)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_sync_sphere_transform(
        nullptr,sphere_id,&transforms,parent)==SAT_ERR_INVALID_ARG);
    CHECK(sat_transform3d_set_parent(
        &transforms,root,SAT_TRANSFORM3D_ROOT)==SAT_OK);
    CHECK(sat_physics3_sync_sphere_transform(
        &physics,sphere_id,&transforms,root)==SAT_OK);
}
int main() {
    sync_root_and_inherit_quaternion();
    std::puts("test_physics3_transform: 1 test passed");
}
