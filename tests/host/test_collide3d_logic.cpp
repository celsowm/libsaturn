#include <cstdio>
#include <cstdlib>
#include "saturn/collide3d.h"
#include "src/physics/3d/collision_logic.hpp"
#define OK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__); std::exit(1); } } while(0)
static sat_fx16_t F(int x){return static_cast<sat_fx16_t>(x*65536);}
int main(){sat_sphere_t a={{F(10000),F(10000),F(10000)},F(8)},b={{F(10005),F(10000),F(10000)},F(8)};sat_contact3_t c;OK(saturn::core::collide3d::sphere_contact(a,b,c));OK(c.depth>0);
 sat_aabb3_t box={{0,0,0},{F(5),F(5),F(5)}};sat_ray3_t ray={{F(-20),0,0},{F(1),0,0},F(40)};sat_hit3_t h;OK(saturn::core::collide3d::ray_box(box,ray,h));OK(h.face==0xFFFF&&h.t>=F(3)/8&&h.t<F(1)/2);
 sat_quad3_t q={{{F(-5),0,F(-5)},{F(5),0,F(-5)},{F(5),0,F(5)},{F(-5),0,F(5)}}};ray.origin={0,F(10),0};ray.dir={0,F(-1),0};ray.length=F(20);OK(saturn::core::collide3d::ray_quad(q,ray,h));OK(h.point.y==0);
 std::puts("PASS: test_collide3d_logic.cpp");return 0;}
