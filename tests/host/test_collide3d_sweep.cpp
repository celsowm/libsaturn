#include <cstdio>
#include <cstdlib>
#include "saturn/collide3d.h"

#define CHECK(x) do { if(!(x)) {std::fprintf(stderr,"FAIL %s:%d: %s\\n",__FILE__,__LINE__,#x);std::exit(1);} } while(0)
#define FX(n) ((sat_fx16_t)((n)*65536))

static void face_interior_cast() {
    sat_vec3_t vertices[8]={
        {-FX(4),0,-FX(4)},{FX(4),0,-FX(4)},
        {FX(4),0,FX(4)},{-FX(4),0,FX(4)},
        {FX(20),0,-FX(4)},{FX(28),0,-FX(4)},
        {FX(28),0,FX(4)},{FX(20),0,FX(4)}
    };
    uint16_t indices[8]={0,1,2,3,4,5,6,7};
    sat_mesh_t mesh{vertices,indices,8,8,2,2};
    sat_sphere_t sphere{{0,FX(3),0},FX(1)/2};
    sat_vec3_t travel{0,-FX(6),0};
    sat_sphere_mesh_face_hit_t hit{};
    uint8_t found=77;
    CHECK(sat_sphere_cast_mesh_faces(
        &mesh,&sphere,&travel,&hit,&found)==SAT_OK);
    CHECK(found==1 && hit.face==0);
    CHECK(hit.t>FX(2)/5 && hit.t<FX(9)/20);
    CHECK(hit.center.y>=FX(1)/2-4 && hit.center.y<=FX(1)/2+4);
    CHECK(hit.point.y>=-4 && hit.point.y<=4);
    CHECK(hit.normal.y>0);

    sphere.center.y=-FX(3);
    travel.y=FX(6);
    CHECK(sat_sphere_cast_mesh_faces(
        &mesh,&sphere,&travel,&hit,&found)==SAT_OK);
    CHECK(found==1 && hit.face==0 && hit.normal.y<0);
    CHECK(hit.center.y>=-FX(1)/2-4 && hit.center.y<=-FX(1)/2+4);

    sphere.center={FX(24),FX(3),0};
    travel.y=-FX(6);
    CHECK(sat_sphere_cast_mesh_faces(
        &mesh,&sphere,&travel,&hit,&found)==SAT_OK);
    CHECK(found==1 && hit.face==1);

    /* Empty gap and outside-the-edge are NOT infinite-plane hits. */
    sphere.center={FX(12),FX(3),0};
    CHECK(sat_sphere_cast_mesh_faces(
        &mesh,&sphere,&travel,&hit,&found)==SAT_OK);
    CHECK(found==0);
    sphere.center={FX(4)+FX(1)/4,FX(3),0};
    CHECK(sat_sphere_cast_mesh_faces(
        &mesh,&sphere,&travel,&hit,&found)==SAT_OK);
    CHECK(found==0);
    sphere.center={0,FX(1)/4,0};
    CHECK(sat_sphere_cast_mesh_faces(
        &mesh,&sphere,&travel,&hit,&found)==SAT_OK);
    CHECK(found==0); /* Initial overlap stays with discrete resolution. */
}
static void cast_rejects_invalid_input() {
    sat_vec3_t vertices[4]={
        {-FX(4),0,-FX(4)},{FX(4),0,-FX(4)},
        {FX(4),0,FX(4)},{-FX(4),0,FX(4)}
    };
    uint16_t indices[4]={0,1,2,3};
    sat_mesh_t mesh{vertices,indices,4,4,1,1};
    const sat_sphere_t sphere{{0,FX(1),0},FX(1)/2};
    const sat_vec3_t delta{0,-FX(2),0};
    sat_sphere_mesh_face_hit_t hit{};
    uint8_t found=77;
    CHECK(sat_sphere_cast_mesh_faces(
        nullptr,&sphere,&delta,&hit,&found)==SAT_ERR_INVALID_ARG);
    CHECK(sat_sphere_cast_mesh_faces(
        &mesh,&sphere,&delta,&hit,nullptr)==SAT_ERR_INVALID_ARG);
    indices[3]=99;
    CHECK(sat_sphere_cast_mesh_faces(
        &mesh,&sphere,&delta,&hit,&found)==SAT_ERR_INVALID_ARG);
}
int main() {
    face_interior_cast();
    cast_rejects_invalid_input();
    std::puts("test_collide3d_sweep: 2 tests passed");
}
