/* Host qualification for the shared, model-size-aware orbit controller. */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "saturn/orbit_camera3d.h"

#define CHECK(v) do { if(!(v)) { std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#v); std::exit(1); } } while(0)
#define EQ(a,b) CHECK((a)==(b))
#define FX(n) (static_cast<sat_fx16_t>((n)*SAT_FX16_ONE))
static sat_fx16_t absfx(sat_fx16_t x) { return x<0 ? -x : x; }

static sat_orbit_camera3d_fit_t make_policy() {
    sat_orbit_camera3d_fit_t p={};
    p.min_extent=SAT_FX16_ONE;
    p.min_distance_floor=FX(10);
    p.initial_distance_factor=FX(3);
    p.min_distance_factor=SAT_FX16_ONE;
    p.max_distance_factor=FX(8);
    p.near_plane_floor=FX(1);
    p.fov_y=FX(60);
    p.aspect=sat_fx16_div(FX(320),FX(224));
    p.far_z=FX(2000);
    p.pitch_min_deg=-60;
    p.pitch_max_deg=60;
    return p;
}
static sat_orbit_camera3d_t static_camera() {
    const sat_vec3_t mn={FX(-4),FX(-2),FX(-1)};
    const sat_vec3_t mx={FX(4),FX(6),FX(3)};
    const sat_orbit_camera3d_fit_t policy=make_policy();
    sat_orbit_camera3d_t camera={};
    EQ(sat_orbit_camera3d_fit_bounds(&camera,&mn,&mx,&policy),SAT_OK);
    return camera;
}
static void static_bounds_and_projection() {
    const sat_orbit_camera3d_t c=static_camera();
    EQ(c.target.x,0);
    EQ(c.target.y,FX(2));
    EQ(c.target.z,FX(1));
    EQ(c.min_distance,FX(10));
    EQ(c.max_distance,FX(64));
    EQ(c.default_distance,FX(24));
    EQ(c.distance,FX(24));
    EQ(c.near_z,FX(1));
    EQ(c.yaw_deg,0);
    EQ(c.pitch_deg,10);
    CHECK(c.eye.z>c.target.z);
    CHECK(c.eye.y>c.target.y);
    CHECK(c.view_proj.m[0]!=0);
}
static void absolute_zoom_floor_does_not_invert_range() {
    const sat_vec3_t origin={0,0,0};
    const sat_orbit_camera3d_fit_t p=make_policy();
    sat_orbit_camera3d_t c={};
    EQ(sat_orbit_camera3d_fit_bounds(&c,&origin,&origin,&p),SAT_OK);
    EQ(c.min_distance,FX(10));
    EQ(c.max_distance,FX(10));
    EQ(c.default_distance,FX(10));
    sat_pad_state_t pad={};
    pad.held=SAT_PAD_R;
    EQ(sat_orbit_camera3d_apply_pad(&c,&pad,SAT_PAD_A),SAT_OK);
    EQ(c.distance,FX(10));
}
static void orbit_zoom_pitch_and_reset() {
    sat_orbit_camera3d_t c=static_camera();
    sat_pad_state_t pad={};
    pad.held=SAT_PAD_RIGHT|SAT_PAD_UP|SAT_PAD_R;
    EQ(sat_orbit_camera3d_apply_pad(&c,&pad,SAT_PAD_A),SAT_OK);
    EQ(c.yaw_deg,2);
    EQ(c.pitch_deg,12);
    CHECK(c.distance<c.default_distance);
    for(int i=0;i<200;++i) {
        pad.held=SAT_PAD_UP|SAT_PAD_R;
        EQ(sat_orbit_camera3d_apply_pad(&c,&pad,SAT_PAD_A),SAT_OK);
    }
    EQ(c.pitch_deg,60);
    EQ(c.distance,c.min_distance);
    pad.held=0u;
    pad.pressed=SAT_PAD_B;
    EQ(sat_orbit_camera3d_apply_pad(&c,&pad,SAT_PAD_A),SAT_OK);
    EQ(c.yaw_deg,0);
    EQ(c.pitch_deg,10);
    EQ(c.distance,c.default_distance);
    EQ(c.auto_orbit,0u);
}
static void automatic_orbit_key_is_owned_by_game() {
    sat_orbit_camera3d_t c=static_camera();
    sat_pad_state_t pad={};
    pad.pressed=SAT_PAD_A;
    EQ(sat_orbit_camera3d_apply_pad(&c,&pad,SAT_PAD_C),SAT_OK);
    EQ(c.auto_orbit,0u);
    pad.pressed=SAT_PAD_C;
    EQ(sat_orbit_camera3d_apply_pad(&c,&pad,SAT_PAD_C),SAT_OK);
    EQ(c.auto_orbit,1u);
    EQ(c.yaw_deg,1);
    pad.pressed=0u;
    for(int i=0;i<359;++i)
        EQ(sat_orbit_camera3d_apply_pad(&c,&pad,SAT_PAD_C),SAT_OK);
    EQ(c.yaw_deg,0);
    pad.pressed=SAT_PAD_C;
    EQ(sat_orbit_camera3d_apply_pad(&c,&pad,SAT_PAD_C),SAT_OK);
    EQ(c.auto_orbit,0u);
    EQ(c.yaw_deg,0);
}
static void animated_union_bounds_support_small_assets() {
    const sat_vec3_t mn={-FX(1)/4,-FX(1)/4,-FX(1)/4};
    const sat_vec3_t mx={FX(1)/4,FX(1)/4,FX(1)/4};
    sat_orbit_camera3d_fit_t p=make_policy();
    p.min_extent=SAT_FX16_ONE>>6;
    p.min_distance_floor=0;
    p.initial_distance_factor=SAT_FX16_ONE+(SAT_FX16_ONE>>2);
    p.min_distance_factor=SAT_FX16_ONE/3;
    p.max_distance_factor=FX(16);
    p.near_plane_factor=SAT_FX16_ONE/8;
    p.near_plane_floor=1;
    sat_orbit_camera3d_t c={};
    EQ(sat_orbit_camera3d_fit_bounds(&c,&mn,&mx,&p),SAT_OK);
    CHECK(c.default_distance<FX(1));
    CHECK(c.near_z<FX(1));
    CHECK(c.min_distance<c.default_distance);
    CHECK(c.max_distance>c.default_distance);
}
static void invalid_inputs_are_atomic() {
    sat_orbit_camera3d_t c=static_camera();
    const sat_orbit_camera3d_t original=c;
    const sat_vec3_t mn={FX(2),0,0},mx={FX(1),0,0};
    const sat_orbit_camera3d_fit_t p=make_policy();
    EQ(sat_orbit_camera3d_fit_bounds(&c,&mn,&mx,&p),SAT_ERR_INVALID_ARG);
    CHECK(std::memcmp(&c,&original,sizeof(c))==0);
    sat_pad_state_t pad={};
    EQ(sat_orbit_camera3d_apply_pad(&c,nullptr,SAT_PAD_A),SAT_ERR_INVALID_ARG);
    EQ(sat_orbit_camera3d_apply_pad(&c,&pad,SAT_PAD_A|SAT_PAD_C),
       SAT_ERR_INVALID_ARG);
    CHECK(std::memcmp(&c,&original,sizeof(c))==0);
    c.near_z=0;
    EQ(sat_orbit_camera3d_update(&c),SAT_ERR_INVALID_ARG);
}
static void oversized_bounds_rejected_before_truncation() {
    const sat_vec3_t mn={INT32_MIN,0,0};
    const sat_vec3_t mx={INT32_MAX,0,0};
    const sat_orbit_camera3d_fit_t p=make_policy();
    sat_orbit_camera3d_t c=static_camera();
    const sat_orbit_camera3d_t original=c;
    EQ(sat_orbit_camera3d_fit_bounds(&c,&mn,&mx,&p),SAT_ERR_INVALID_ARG);
    CHECK(std::memcmp(&c,&original,sizeof(c))==0);
}
int main() {
    static_bounds_and_projection();
    absolute_zoom_floor_does_not_invert_range();
    orbit_zoom_pitch_and_reset();
    automatic_orbit_key_is_owned_by_game();
    animated_union_bounds_support_small_assets();
    invalid_inputs_are_atomic();
    oversized_bounds_rejected_before_truncation();
    puts("test_orbit_camera3d: 7 tests passed");
    return 0;
}
