/* physics_3d - low-poly 3D arcade motion, static AABBs and ray picking. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/collide3d.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/fmt.h"
#include "saturn/hud.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/mesh3d.h"
#include "saturn/physics.h"
#include "saturn/scene.h"
#include "saturn/spatial.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"

#define BALLS 8
#define SCENE_FACE_CAPACITY 512u
#define FX(v) ((sat_fx16_t)((int32_t)(v) * 65536))
static sat_body3_t g_balls[BALLS];
static sat_aabb3_t g_boxes[5];
static sat_mesh_t g_meshes[5];
static sat_vec3_t g_box_vertices[5][8];
static uint16_t g_box_indices[5][24];
static sat_vec3_t g_sphere_vertices[64];
static uint16_t g_sphere_indices[192];
static sat_mesh_t g_sphere_mesh;
static sat_camera3d_t g_camera;
static sat_scene_t g_scene;
static sat_hud_t g_hud;
static sat_scene3d_face_t g_scene_faces[SCENE_FACE_CAPACITY];
static uint32_t g_scene_keys[SCENE_FACE_CAPACITY];
static uint16_t g_scene_order[SCENE_FACE_CAPACITY];
static uint16_t g_face_materials[48];
static sat_projected_vertex_t g_mesh_screen[64];
static sat_ascii_font_t g_font;
static uint16_t g_hit_face;
static uint16_t g_pair_count;
static uint16_t g_spatial_heads[7 * 5];
static sat_spatial_entry_t g_spatial_entries[BALLS * 4];
static uint16_t g_spatial_stamps[BALLS];
static sat_box2_t g_footprints[BALLS];
static sat_spatial_pair_t g_pairs[32];
static sat_spatial_t g_spatial;

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
    sat_spatial_clear(&g_spatial);
    for (i=0;i<BALLS;++i) {
        sat_body3_step(&g_balls[i],&p);
        sat_body3_collide_aabbs(&g_balls[i],g_boxes,5);
        if (g_balls[i].flags & SAT_BODY3_GROUNDED) g_balls[i].vel.y=FX(4+(i&1));
        g_footprints[i] = (sat_box2_t){{g_balls[i].shape.center.x + FX(96), g_balls[i].shape.center.z + FX(80)}, {g_balls[i].shape.radius, g_balls[i].shape.radius}};
        sat_example_must(sat_spatial_insert(&g_spatial,i,&g_footprints[i]));
    }
    sat_example_must(sat_spatial_pairs(&g_spatial,g_pairs,32,&g_pair_count));
    for (i=0;i<g_pair_count;++i) sat_body3_separate(&g_balls[g_pairs[i].a],&g_balls[g_pairs[i].b]);
}
static void draw_mesh(const sat_mesh_t* m,uint16_t color,uint16_t pass) {
    sat_scene3d_material_t material = {};
    sat_scene3d_instance_t instance = {};
    material.kind = SAT_SCENE3D_RGB;
    material.rgb555 = color;
    material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    instance.mesh = m;
    instance.materials = &material;
    instance.material_count = 1u;
    instance.face_materials = g_face_materials;
    instance.world = NULL;
    instance.pass = pass;
    instance.cull_backfaces = 0u;
    sat_example_must(sat_scene_submit_instance(
        &g_scene, &instance, SAT_SCENE3D_SLOT_INHERIT, g_mesh_screen, NULL));
}
static void draw_scene(void) {
    uint16_t i;
    sat_quad3_t floor;
    sat_scene3d_material_t floor_material = {};
    floor_material.kind = SAT_SCENE3D_RGB;
    floor_material.rgb555 = SAT_RGB555(3,6,14);
    floor_material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    sat_quad3_floor(&floor,0,FX(-4),0,FX(95));
    sat_example_must(sat_scene_submit_quad(&g_scene,&floor,&floor_material,0u));
    for(i=0;i<5;++i) draw_mesh(&g_meshes[i],i==g_hit_face?SAT_RGB555(31,20,4):SAT_RGB555(6,12,27),1u);
    for(i=0;i<BALLS;++i){sat_mesh_build_sphere(&g_sphere_mesh,&g_balls[i].shape.center,g_balls[i].shape.radius,6,3);draw_mesh(&g_sphere_mesh,i==0?SAT_RGB555(31,26,3):SAT_RGB555(4,24,31),1u);}
}
static void hud(void) {
    sat_example_must(sat_hud_text(&g_hud, "PHYSICS 3D  MOVE: D-PAD + B", 8, 4));
    sat_example_must(sat_hud_value(&g_hud, "BODIES ", BALLS, 8, 14));
    sat_example_must(sat_hud_value(&g_hud, "PAIRS ", g_pair_count, 96, 14));
    sat_example_must(sat_hud_value(&g_hud, "HIT FACE ", g_hit_face, 8, 24));
    sat_example_must(sat_hud_text(&g_hud, "START: RESET", 112, 24));
}
int main(void){sat_vec3_t eye={FX(0),FX(92),FX(135)},center={0,FX(4),0},up={0,FX(1),0};sat_ray3_t ray; sat_step_clock_t clock;uint16_t steps;
    {sat_example_must(sat_app_init_default());sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font,SAT_COLOR_WHITE,SAT_COLOR_BLACK,1));sat_example_must(sat_hud_init(&g_hud,&g_font,SAT_COLOR_WHITE,8u));sat_example_must(sat_camera3d_init(&g_camera,&eye,&center,&up,FX(42),sat_fx16_div(FX(320),FX(224)),FX(4),FX(500)));sat_example_must(sat_scene_init(&g_scene,g_scene_faces,g_scene_keys,g_scene_order,SCENE_FACE_CAPACITY));sat_example_must(sat_spatial_init(&g_spatial,g_spatial_heads,7,5,5,g_spatial_entries,BALLS*4,g_spatial_stamps,g_footprints,BALLS));}init_scene();sat_step_clock_init(&clock);
    for(;;){sat_pad_state_t pad={0};sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,&pad));steps=sat_step_clock_steps(&clock,3);while(steps--)simulate(&pad);ray.origin=eye;ray.dir=(sat_vec3_t){0,FX(-1),FX(-2)};sat_vec3_normalize(&ray.dir,&ray.dir);ray.length=FX(500);g_hit_face=0xFFFF;{sat_hit3_t h;for(uint16_t i=0;i<5;++i)if(sat_raycast_aabb3(&g_boxes[i],&ray,&h)&&g_hit_face==0xFFFF)g_hit_face=i;}sat_example_must(sat_scene_begin(&g_scene,&g_camera,FX(4),320u,224u,48u));draw_scene();sat_example_must(sat_scene_flush(&g_scene));hud();sat_example_must(sat_app_frame_end());}
}
