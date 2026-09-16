/* physics_3d - low-poly 3D arcade motion, static AABBs and ray picking. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/collide3d.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/fmt.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/mesh3d.h"
#include "saturn/physics.h"
#include "saturn/render3d.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"

#define BALLS 8
#define FX(v) ((sat_fx16_t)((int32_t)(v) * 65536))
static sat_body3_t g_balls[BALLS];
static sat_aabb3_t g_boxes[5];
static sat_mesh_t g_meshes[5];
static sat_vec3_t g_box_vertices[5][8];
static uint16_t g_box_indices[5][24];
static sat_vec3_t g_sphere_vertices[64];
static uint16_t g_sphere_indices[192];
static sat_mesh_t g_sphere_mesh;
static sat_mat4_t g_view_proj;
static sat_ascii_font_t g_font;
static uint16_t g_hit_face;

static void init_scene(void) {
    uint16_t i;
    g_boxes[0] = (sat_aabb3_t){{0,FX(-8),0},{FX(90),FX(4),FX(70)}};
    g_boxes[1] = (sat_aabb3_t){{FX(-45),FX(6),FX(-5)},{FX(6),FX(14),FX(20)}};
    g_boxes[2] = (sat_aabb3_t){{FX(35),FX(4),FX(-15)},{FX(14),FX(12),FX(6)}};
    g_boxes[3] = (sat_aabb3_t){{FX(-25),FX(3),FX(28)},{FX(18),FX(11),FX(5)}};
    g_boxes[4] = (sat_aabb3_t){{FX(55),FX(5),FX(35)},{FX(5),FX(13),FX(20)}};
    for (i=0;i<5;++i) sat_example_must(sat_mesh_init(&g_meshes[i],g_box_vertices[i],8,g_box_indices[i],6));
    for (i=0;i<5;++i) sat_example_must(sat_mesh_build_box(&g_meshes[i],&g_boxes[i].center,g_boxes[i].half.x,g_boxes[i].half.y,g_boxes[i].half.z));
    sat_example_must(sat_mesh_init(&g_sphere_mesh,g_sphere_vertices,64,g_sphere_indices,48));
    for (i=0;i<BALLS;++i) { g_balls[i].shape.center=(sat_vec3_t){FX((int)i*11-38),FX(12+(i%3)*8),FX((int)i*7-24)};g_balls[i].shape.radius=FX(4);g_balls[i].vel=(sat_vec3_t){FX((int)(i%3)-1),FX(0),FX((int)(i%4)-2)};g_balls[i].flags=0; }
}
static void simulate(const sat_pad_state_t* pad) {
    uint16_t i;
    sat_body3_params_t p={{0,FX(-1)/8,0},FX(7),FX(255)/256,FX(3)/4,FX(3)/4};
    if (pad->pressed & SAT_PAD_START) init_scene();
    if (pad->pressed & SAT_PAD_B) g_balls[0].vel.y=FX(5);
    if (pad->held & SAT_PAD_LEFT) g_balls[0].vel.x=FX(-2); else if (pad->held & SAT_PAD_RIGHT) g_balls[0].vel.x=FX(2);
    if (pad->held & SAT_PAD_UP) g_balls[0].vel.z=FX(-2); else if (pad->held & SAT_PAD_DOWN) g_balls[0].vel.z=FX(2);
    for (i=0;i<BALLS;++i) { sat_body3_step(&g_balls[i],&p); sat_body3_collide_aabbs(&g_balls[i],g_boxes,5); if (g_balls[i].flags & SAT_BODY3_GROUNDED) g_balls[i].vel.y=FX(4+(i&1)); }
    for (i=0;i<BALLS;++i) for (uint16_t j=i+1;j<BALLS;++j) sat_body3_separate(&g_balls[i],&g_balls[j]);
}
static void draw_mesh(const sat_mesh_t* m,uint16_t color) { uint16_t i; for(i=0;i<m->face_count;++i){sat_quad3_t q;if(sat_mesh_face_quad(m,i,&q)==SAT_OK) sat_draw_world_polygon(&g_view_proj,&q,color);} }
static void draw_scene(void) {
    uint16_t i;
    sat_quad3_t floor; sat_quad3_floor(&floor,0,FX(-4),0,FX(95)); sat_draw_world_polygon(&g_view_proj,&floor,SAT_RGB555(3,6,14));
    for(i=0;i<5;++i) draw_mesh(&g_meshes[i],i==g_hit_face?SAT_RGB555(31,20,4):SAT_RGB555(6,12,27));
    for(i=0;i<BALLS;++i){sat_mesh_build_sphere(&g_sphere_mesh,&g_balls[i].shape.center,g_balls[i].shape.radius,6,3);draw_mesh(&g_sphere_mesh,i==0?SAT_RGB555(31,26,3):SAT_RGB555(4,24,31));}
}
static void hud(void) { char text[40]; sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font,"PHYSICS 3D  D-PAD MOVE  B JUMP  START RESET",8,6,0,1,0));sat_example_must(sat_fmt_label_u32("BODIES ",BALLS,text,sizeof(text),0));sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font,text,8,16,0,1,0));sat_example_must(sat_fmt_label_u32("HIT FACE ",g_hit_face,text,sizeof(text),0));sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font,text,100,16,0,1,0)); }
int main(void){sat_vec3_t eye={FX(0),FX(92),FX(135)},center={0,FX(4),0},up={0,FX(1),0};sat_ray3_t ray; sat_step_clock_t clock;uint16_t steps;
    {sat_mat4_t view,proj;sat_example_must(sat_app_init_default());sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,1));sat_example_must(sat_mat4_look_at(&view,&eye,&center,&up));sat_example_must(sat_mat4_perspective(&proj,FX(42),sat_fx16_div(FX(320),FX(224)),FX(4),FX(500)));sat_example_must(sat_mat4_multiply(&g_view_proj,&proj,&view));}init_scene();sat_step_clock_init(&clock);
    for(;;){sat_pad_state_t pad={0};sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,&pad));steps=sat_step_clock_steps(&clock,3);while(steps--)simulate(&pad);ray.origin=eye;ray.dir=(sat_vec3_t){0,FX(-1),FX(-2)};sat_vec3_normalize(&ray.dir,&ray.dir);ray.length=FX(500);g_hit_face=0xFFFF;{sat_hit3_t h;for(uint16_t i=0;i<5;++i)if(sat_raycast_aabb3(&g_boxes[i],&ray,&h)&&g_hit_face==0xFFFF)g_hit_face=i;}draw_scene();hud();sat_example_must(sat_app_frame_end());}
}
