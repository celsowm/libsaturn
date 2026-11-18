#include <stdint.h>
#include "saturn/saturn.h"
#include "saturn/app.h"
#include "saturn/scene.h"
#include "saturn/mesh3d.h"
#include "saturn/model3d.h"
#include "saturn/orbit_camera3d.h"
#include "saturn/time.h"
#include "saturn/color.h"
#include "example_util.h"
#include "src/hal/vdp1/vdp1.hpp"
#include "world_cull_renderer/sonic_model.h"

extern "C" {
extern const uint32_t g_world_cull_mode;
extern uint32_t g_world_cull_census[12];
}
namespace {
constexpr unsigned Steps=16,Cap=256,GCap=32,VertexCap=320,FaceCap=200,TextureCap=64;
struct Row {
    uint32_t step,pose,path,submit,projection,prepare,flush,end,commands,gouraud,
             culled,faces,timeouts,serial,census[12];
};
struct Report {
    uint32_t magic,version,mode,frequency,count,done,mesh_faces,vertices;
    Row rows[Steps];
};
static_assert(sizeof(Report)==1696 && sizeof(saturn::hal::vdp1::Command)==32);
static sat_vec3_t vertices[VertexCap];
static uint16_t indices[FaceCap*4],face_materials[FaceCap];
static uint8_t double_sided[FaceCap];
static sat_mesh_t mesh;
static sat_projected_vertex_t screen[VertexCap];
static sat_scene3d_face_t entries[FaceCap];
static uint32_t keys[FaceCap];
static uint16_t order[FaceCap];
static sat_vdp1_texture_t textures[TextureCap];
static sat_scene3d_material_t materials[TextureCap];
static sat_scene3d_material_program_t prefixes[TextureCap];
static sat_scene3d_material_programs_t programs;
static sat_scene3d_instance_binding_t bindings[2];
static sat_scene3d_instance_t instance;
static sat_scene_t scene;
static sat_orbit_camera3d_t orbit;
uint32_t now(){return uint32_t(sat_time_ticks());}
void initialize(){
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_model_validate(&sonic_model_asset));
    sat_example_must(sat_mesh_init(&mesh,vertices,VertexCap,indices,FaceCap));
    sat_example_must(sat_model_copy_to_mesh(&sonic_model_asset,&mesh));
    sat_example_must(sat_model_upload_textures(&sonic_model_asset,textures,TextureCap));
    for(unsigned i=0;i<sonic_model_asset.texture_count;++i){
        materials[i].kind=SAT_SCENE3D_INDEXED_TEXTURED;
        materials[i].texture=&textures[i];
        materials[i].color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    }
    for(unsigned i=0;i<mesh.face_count;++i){
        face_materials[i]=sonic_model_asset.face_texture_indices[i];
        // Diagnostic mask, not an attribute of the original asset.
        double_sided[i]=(i%17==0);
    }
    sat_example_must(sat_scene_init(&scene,entries,keys,order,FaceCap));
    for(auto& b:bindings)sat_example_must(sat_scene3d_instance_binding_init(
        &b,&mesh,materials,sonic_model_asset.texture_count,face_materials));
    sat_example_must(sat_scene3d_material_programs_init(&programs,materials,
        sonic_model_asset.texture_count,prefixes,TextureCap));
    sat_example_must(programs.all_compiled?SAT_OK:SAT_ERR_VERIFY_FAILED);
    bindings[1].programs=&programs;
    instance.mesh=&mesh;instance.materials=materials;
    instance.material_count=sonic_model_asset.texture_count;
    instance.face_materials=face_materials;instance.cull_backfaces=1;
    instance.double_sided_faces=double_sided;
    sat_vec3_t mn{},mx{};
    sat_example_must(sat_model_compute_bounds(&sonic_model_asset,&mn,&mx));
    sat_orbit_camera3d_fit_t fit{};
    fit.min_extent=SAT_FX16_ONE;
    fit.min_distance_floor=10*SAT_FX16_ONE;
    fit.initial_distance_factor=3*SAT_FX16_ONE;
    fit.min_distance_factor=SAT_FX16_ONE;
    fit.max_distance_factor=8*SAT_FX16_ONE;
    fit.near_plane_floor=SAT_FX16_ONE;
    fit.fov_y=60*SAT_FX16_ONE;
    fit.aspect=sat_fx16_div(320*SAT_FX16_ONE,224*SAT_FX16_ONE);
    fit.far_z=2000*SAT_FX16_ONE;
    fit.pitch_min_deg=-60;fit.pitch_max_deg=60;
    sat_example_must(sat_orbit_camera3d_fit_bounds(&orbit,&mn,&mx,&fit));
}
}
extern "C" {
Report g_world_cull_renderer;
uint16_t g_world_cull_commands[Steps][Cap*16];
uint16_t g_world_cull_gouraud[Steps][GCap*4];
}
extern "C" void world_cull_renderer(void){
    initialize();
    auto& report=g_world_cull_renderer;
    report.magic=0x57435231u;report.version=1;report.mode=g_world_cull_mode;
    report.frequency=sat_time_frequency_hz();report.mesh_faces=mesh.face_count;report.vertices=mesh.vertex_count;
    for(;;){
        const unsigned step=report.count,index=step<Steps?step:Steps-1;
        const unsigned pose=(index/2)%4,path=index/8;
        orbit.yaw_deg=int(pose)*45;orbit.pitch_deg=10;
        sat_example_must(sat_orbit_camera3d_update(&orbit));
        sat_camera3d_t camera{};
        camera.eye=orbit.eye;camera.target=orbit.target;
        camera.up={0,SAT_FX16_ONE,0};camera.view_proj=orbit.view_proj;
        instance.validated_binding=&bindings[path];
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,nullptr));
        sat_example_must(sat_scene_begin(&scene,&camera,orbit.near_z,320,224,0));
        for(auto& c:g_world_cull_census)c=0;
        Row row{};row.step=index;row.pose=pose;row.path=path;
        uint32_t stamp=now();
        sat_example_must(sat_scene_submit_instance(&scene,&instance,SAT_SCENE3D_SLOT_INHERIT,screen,nullptr));
        row.submit=now()-stamp;
        row.projection=scene.faces.instance_profile.projection_ticks;
        row.prepare=scene.faces.instance_profile.preparation_ticks;
        for(unsigned i=0;i<12;++i)row.census[i]=g_world_cull_census[i];
        stamp=now();
        sat_example_must(sat_scene_flush(&scene));row.flush=now()-stamp;
        row.culled=scene.culled_faces;row.faces=scene.flushed_faces;
        // A constant shaded RGB swatch exercises table capture; it is
        // independent of the textured model and outside measured submit.
        sat_quad2_t swatch{};
        swatch.x[0]=swatch.x[3]=-152;swatch.x[1]=swatch.x[2]=-124;
        swatch.y[0]=swatch.y[1]=88;swatch.y[2]=swatch.y[3]=104;
        const uint16_t shades[4]={SAT_RGB555(10,12,16),SAT_RGB555(18,20,16),
                                  SAT_RGB555(18,16,20),SAT_RGB555(12,14,18)};
        sat_example_must(sat_draw_quad2_polygon_gouraud(&swatch,SAT_RGB555(14,14,14),shades));
        sat_vdp1_command_checkpoint_t ck{};
        sat_example_must(sat_vdp1_command_checkpoint(&ck));
        sat_example_must(ck.used<Cap&&ck.gouraud_tables<=GCap?SAT_OK:SAT_ERR_CAPACITY);
        auto* words=reinterpret_cast<volatile uint16_t*>(saturn::hal::vdp1::frame_commands());
        auto* tables=words+32768;
        row.commands=ck.used+1;row.gouraud=ck.gouraud_tables;
        if(step<Steps){
            for(unsigned j=0;j<unsigned(ck.used)*16;++j)g_world_cull_commands[step][j]=words[j];
            for(unsigned j=0;j<unsigned(ck.gouraud_tables)*4;++j)g_world_cull_gouraud[step][j]=tables[j];
        }
        const uint32_t serial=sat_vdp1_list_serial();
        stamp=now();sat_example_must(sat_app_frame_end());row.end=now()-stamp;
        row.serial=sat_vdp1_list_serial();
        sat_example_must(row.serial==serial+1?SAT_OK:SAT_ERR_VERIFY_FAILED);
        sat_vdp1_frame_stats_t stats{};
        sat_example_must(sat_vdp1_frame_stats(&stats));row.timeouts=stats.draw_timeouts;
        if(step<Steps){
            for(unsigned j=unsigned(ck.used)*16;j<row.commands*16;++j)g_world_cull_commands[step][j]=words[j];
            report.rows[step]=row;
            asm volatile("":::"memory");report.count=step+1;
            if(report.count==Steps){asm volatile("":::"memory");report.done=1;}
        }
    }
}
