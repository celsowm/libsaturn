#include <cstdio>
#include <cstdlib>
#include "saturn/collide2d.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__); std::exit(1); } } while(0)
#define EQ(a,b) OK((a)==(b))
static sat_fx16_t F(int x) { return static_cast<sat_fx16_t>(x * 65536); }
int main() {
    sat_box2_t a={{0,0},{F(5),F(4)}}, b={{F(9),0},{F(5),F(4)}}, edge={{F(10),0},{F(5),F(4)}}; sat_contact2_t c;
    OK(sat_box2_overlap(&a,&b)); EQ(sat_box2_overlap(&a,&edge),0);
    OK(sat_box2_contact(&a,&b,&c)); EQ(c.normal.x,-65536); OK(c.depth==F(1));
    sat_circle_t ca={{0,0},F(5)}, cb={{F(9),0},F(5)}; OK(sat_circle_overlap(&ca,&cb));
    OK(sat_circle_contact(&ca,&cb,&c)); OK(c.depth>F(0));
    sat_vec2_t o={F(-20),0},d={F(40),0};sat_hit2_t h;OK(sat_raycast_box2(&a,&o,&d,&h));OK(h.t>=F(3)/8&&h.t<F(1)/2);EQ(h.normal.x,-65536);
    sat_box2_t expanded={{F(10),0},{F(2),F(2)}};sat_vec2_t delta={F(20),0};OK(sat_sweep_box2(&a,&delta,&expanded,&h));OK(h.t>=0&&h.t<=65536);
    EQ(sat_approach(F(1),F(3),F(1)),F(2));sat_vec2_t rv=sat_reflect2((sat_vec2_t){F(2),0},(sat_vec2_t){-F(1),0},0);OK(rv.x<0);
    std::puts("PASS: test_collide2d_logic.cpp"); return 0;
}
