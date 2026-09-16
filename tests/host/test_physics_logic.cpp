#include <cstdio>
#include <cstdlib>
#include "saturn/physics.h"
#include "src/core/physics_logic.hpp"
#define OK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d\n",__FILE__,__LINE__); std::exit(1); } } while(0)
static sat_fx16_t F(int x){return static_cast<sat_fx16_t>(x*65536);}
static uint32_t g_frames;
extern "C" uint32_t sat_frame_count(void) { return g_frames; }
static int tiles(int c,int r,void*){return (r==3&&c>=0&&c<8)?SAT_TILE_SOLID:SAT_TILE_EMPTY;}
int main(){sat_step_clock_t clock;g_frames=100;saturn::core::physics::clock_init(clock);g_frames=105;OK(saturn::core::physics::clock_steps(clock,8)==5);g_frames=130;OK(saturn::core::physics::clock_steps(clock,4)==4);
 sat_body2_t b={{{F(12),F(20)},{F(3),F(3)}},{0,F(20)},0};sat_grid_t g={8,6,8,0,0,0,0};OK(saturn::core::physics::move_tiles(b,g,tiles,0)==SAT_OK);OK((b.flags&SAT_BODY_GROUNDED)!=0);OK(b.box.center.y==F(21));
 sat_body2_t a={{{0,0},{F(4),F(4)}},{0,0},0},d={{{F(6),0},{F(4),F(4)}},{0,0},0};OK(saturn::core::physics::separate(a,d));OK(!sat_box2_overlap(&a.box,&d.box));
 std::puts("PASS: test_physics_logic.cpp");return 0;}
