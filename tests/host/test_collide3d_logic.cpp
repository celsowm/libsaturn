#include <cstdio>
#include <cstdlib>
#include "saturn/collide3d.h"
#include "src/physics/3d/collision_logic.hpp"
#define OK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__); std::exit(1); } } while(0)
static sat_fx16_t F(int x){return static_cast<sat_fx16_t>(x*65536);}
int main(){sat_sphere_t a={{F(10000),F(10000),F(10000)},F(8)},b={{F(10005),F(10000),F(10000)},F(8)};sat_contact3_t c;OK(saturn::core::collide3d::sphere_contact(a,b,c));OK(c.depth>0);
 sat_aabb3_t box={{0,0,0},{F(5),F(5),F(5)}};sat_ray3_t ray={{F(-20),0,0},{F(1),0,0},F(40)};sat_hit3_t h;OK(saturn::core::collide3d::ray_box(box,ray,h));OK(h.face==0xFFFF&&h.t>=F(3)/8&&h.t<F(1)/2);
 sat_quad3_t q={{{F(-5),0,F(-5)},{F(5),0,F(-5)},{F(5),0,F(5)},{F(-5),0,F(5)}}};ray.origin={0,F(10),0};ray.dir={0,F(-1),0};ray.length=F(20);OK(saturn::core::collide3d::ray_quad(q,ray,h));OK(h.point.y==0);

 sat_vec3_t vertices[4]={q.v[0],q.v[1],q.v[2],q.v[3]};
 uint16_t indices[4]={0u,1u,2u,3u};
 sat_mesh_t mesh={};mesh.vertices=vertices;mesh.indices=indices;
 mesh.vertex_count=4u;mesh.face_count=1u;
 sat_quad3_t extracted={};
 OK(saturn::core::geometry::face_quad(&mesh,0u,&extracted)==SAT_OK);
 OK(extracted.v[2].z==q.v[2].z);
 const sat_vec3_t normal=saturn::core::geometry::quad_normal_scaled(extracted);
 OK(normal.y>0 && normal.x==0 && normal.z==0);
 OK(saturn::core::collide3d::ray_mesh(mesh,ray,h));
 OK(h.face==0u && h.point.y==0);
 indices[2]=4u;
 OK(saturn::core::geometry::face_quad(&mesh,0u,&extracted)==SAT_ERR_INVALID_ARG);
 OK(saturn::core::geometry::face_quad(&mesh,1u,&extracted)==SAT_ERR_INVALID_ARG);
 std::puts("PASS: test_collide3d_logic.cpp");return 0;}
