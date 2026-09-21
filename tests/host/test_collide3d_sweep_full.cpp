#include <cstdio>
#include <cstdlib>
#include "saturn/collide3d.h"

#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);}}while(0)
#define FX(n) ((sat_fx16_t)((n)*65536))
static sat_vec3_t vertices[4]={
    {-FX(4),0,-FX(4)},{FX(4),0,-FX(4)},
    {FX(4),0,FX(4)},{-FX(4),0,FX(4)}
};
static uint16_t indices[4]={0,1,2,3};
static sat_mesh_t mesh{vertices,indices,4,4,1,1};

static sat_sphere_mesh_hit_t sweep(
    sat_vec3_t center, sat_vec3_t delta, sat_fx16_t radius,
    uint8_t expected_feature) {
    const sat_sphere_t sphere{center,radius};
    sat_sphere_mesh_hit_t hit{};
    uint8_t found=0;
    CHECK(sat_sphere_cast_mesh(&mesh,&sphere,&delta,&hit,&found)==SAT_OK);
    CHECK(found==1);
    CHECK(hit.feature==expected_feature);
    CHECK(hit.face==0);
    CHECK(hit.t<=SAT_FX16_ONE);
    return hit;
}
static void face_edge_vertex() {
    const sat_vec3_t down{0,-FX(6),0};
    const sat_sphere_mesh_hit_t face=sweep(
        {0,FX(3),0},down,FX(1)/2,0);
    CHECK(face.normal.y>FX(99)/100);
    CHECK(face.point.y==0);
    CHECK(face.center.y>=FX(1)/2-16 && face.center.y<=FX(1)/2+16);

    const sat_sphere_mesh_hit_t edge=sweep(
        {FX(4)+FX(1)/4,FX(3),0},down,FX(1)/2,1);
    CHECK(edge.point.x==FX(4) && edge.point.y==0);
    CHECK(edge.normal.x>0 && edge.normal.y>0);
    CHECK(edge.t>face.t);

    const sat_sphere_mesh_hit_t corner=sweep(
        {FX(4)+FX(1)/4,FX(3),FX(4)+FX(1)/4},
        down,FX(1)/2,2);
    CHECK(corner.point.x==FX(4) && corner.point.z==FX(4));
    CHECK(corner.normal.x>0 && corner.normal.y>0 && corner.normal.z>0);
    CHECK(corner.t>edge.t);

    const sat_sphere_mesh_hit_t from_below=sweep(
        {0,-FX(3),0},{0,FX(6),0},FX(1)/2,0);
    CHECK(from_below.normal.y<0);
}
static void fast_edge_side_approach() {
    const sat_sphere_mesh_hit_t side=sweep(
        {FX(7),FX(1)/4,0},{-FX(6),0,0},FX(1)/2,1);
    CHECK(side.normal.x>0 && side.normal.y>0);
    CHECK(side.t>0 && side.t<SAT_FX16_ONE);
}
static void preserves_real_gaps_and_rejects_invalid_mesh() {
    const sat_vec3_t down{0,-FX(6),0};
    const sat_sphere_t sphere{{FX(5),FX(3),0},FX(1)/2};
    sat_sphere_mesh_hit_t hit{};
    uint8_t found=55;
    CHECK(sat_sphere_cast_mesh(&mesh,&sphere,&down,&hit,&found)==SAT_OK);
    CHECK(found==0);
    const sat_sphere_t start_overlapping{{0,FX(1)/4,0},FX(1)/2};
    CHECK(sat_sphere_cast_mesh(&mesh,&start_overlapping,&down,&hit,&found)==SAT_OK);
    CHECK(found==0);
    CHECK(sat_sphere_cast_mesh(nullptr,&sphere,&down,&hit,&found)==SAT_ERR_INVALID_ARG);
    indices[3]=9;
    CHECK(sat_sphere_cast_mesh(&mesh,&sphere,&down,&hit,&found)==SAT_ERR_INVALID_ARG);
    indices[3]=3;
}
int main() {
    face_edge_vertex();
    fast_edge_side_approach();
    preserves_real_gaps_and_rejects_invalid_mesh();
    std::puts("test_collide3d_sweep_full: 3 tests passed");
}
