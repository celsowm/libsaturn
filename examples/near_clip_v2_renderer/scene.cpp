#include <stdint.h>
#include "saturn/saturn.h"
#include "saturn/app.h"
#include "saturn/time.h"
#include "saturn/color.h"
#include "example_util.h"
#include "src/hal/vdp1/vdp1.hpp"

extern "C" {
extern const uint32_t g_near_clip_mode;
extern volatile uint32_t g_near_clip_ticks,g_near_clip_calls,g_near_clip_partial,g_near_clip_pieces;
}
namespace {
constexpr unsigned Steps=32,Cap=256,GCap=256;
struct Row {
    uint32_t step,pose,clip_ticks,calls,multi_piece,pieces,submit_ticks,
             end_ticks,commands,gouraud,draw_timeouts,serial;
};
struct Report {
    uint32_t magic,version,mode,frequency,count,done,command_cap,gouraud_cap;
    Row rows[Steps];
};
static_assert(sizeof(Report)==1568);
static_assert(sizeof(saturn::hal::vdp1::Command)==32);
static sat_vdp1_texture_t uniform;
static uint8_t texels[16*16];
static uint16_t palette[256];
static sat_mat4_t projection;
static sat_indexed_solid_render3d_t camera;
static constexpr uint16_t colors[]={
    SAT_RGB555(7,14,23),SAT_RGB555(12,24,25),SAT_RGB555(29,18,6),
    SAT_RGB555(12,23,11),SAT_RGB555(18,9,24),SAT_RGB555(24,10,10)};
uint32_t now(){return uint32_t(sat_time_ticks());}
// A consistent Q16 view basis, either axial or yawed about Y.
// Build geometry in camera coordinates then transform it into world
// coordinates. All points use positive clip W; near depth is two units.
constexpr int32_t F=65536;
static int32_t cosine=F,sine=0;
void setup_pose(unsigned pose){
    cosine=pose<8?F:52429;
    sine=pose<8?0:39322;
    projection.m[0]=cosine;projection.m[2]=-sine;
    projection.m[5]=F;
    projection.m[12]=sine;projection.m[14]=cosine;
    camera.forward={sine,0,cosine};
}
sat_vec3_t point(int x,int y,int z){
    const int32_t cx=int32_t(int64_t(x)*z*F/160);
    const int32_t cy=int32_t(-int64_t(y)*z*F/112);
    const int32_t cz=z*F;
    return {int32_t((int64_t(cosine)*cx+int64_t(sine)*cz)/F),
            cy,int32_t((-int64_t(sine)*cx+int64_t(cosine)*cz)/F)};
}
sat_quad3_t face(int x0,int y0,int x1,int y1,int zl,int zr,int skew=0){
    return {{point(x0,y0,zl),point(x1,y0+skew,zr),
             point(x1,y1,zr),point(x0,y1-skew,zl)}};
}
void draw_scene(unsigned pose){
    const bool crossing=pose<4||pose>=12;
    const int dx=int(pose%4)*9-14,dy=int(pose%4)*5-7;
    auto q=face(-260,-180,260,180,12,12);
    sat_example_must(sat_draw_polygon_quad3(&q,&camera,SAT_RGB555(3,5,12)));
    for(unsigned i=0;i<24;++i){
        const int x=int(i%6)*64-192+dx;
        const int y=int(i/6)*58-114+dy;
        // Vertically parallel edges at different positive depths: planar
        // convex world quads. Vary crossing direction and material.
        const bool cut=crossing&&(i%4!=3);
        const int zl=cut&&!(i&1)?1:4;
        const int zr=cut&&(i&1)?1:4;
        q=face(x,y,x+56,y+45,zl,zr,int(i%5)*2-4);
        if(i%3==0){
            sat_example_must(sat_draw_polygon_quad3(&q,&camera,colors[i%6]));
        }else if(i%3==1){
            sat_example_must(sat_draw_indexed_solid_quad3(&q,&camera,&uniform));
        }else{
            const uint16_t g[4]={SAT_RGB555(10,13,16),SAT_RGB555(20,17,13),
                SAT_RGB555(18,21,23),SAT_RGB555(8,12,16)};
            sat_example_must(sat_draw_polygon_quad3_gouraud(&q,&camera,colors[i%6],g));
        }
    }
    q=face(-215+dx,58+dy,220+dx,125+dy,5,5,12);
    sat_example_must(sat_draw_polygon_quad3(&q,&camera,SAT_RGB555(11,16,23)));
    q=face(-190-dx,-122,-126-dx,100,4,4,4);
    sat_example_must(sat_draw_indexed_solid_quad3(&q,&camera,&uniform));
    q=face(105+dx,-135,190+dx,105,4,4,-7);
    sat_example_must(sat_draw_polygon_quad3(&q,&camera,SAT_RGB555(22,13,8)));
    // All-behind-near reject in every phase. Positive W is not sufficient.
    q=face(-20,-20,20,20,1,1);
    sat_example_must(sat_draw_polygon_quad3(&q,&camera,SAT_RGB555(31,0,0)));
}
}
extern "C" {
Report g_near_clip_v2_renderer;
uint16_t g_near_clip_commands[Steps][Cap*16];
uint16_t g_near_clip_gouraud[Steps][GCap*4];
}
extern "C" void near_clip_v2_renderer(void){
    sat_example_must(sat_app_init_default());
    for(auto& v:texels)v=1;
    palette[1]=SAT_RGB555(12,20,25);
    sat_example_must(sat_tex_upload_indexed8(&uniform,texels,16,16,palette,2));
    camera.view_proj=&projection;camera.eye={0,0,0};
    camera.near_depth=2*F;camera.width=320;camera.height=224;
    camera.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    auto& report=g_near_clip_v2_renderer;
    report.magic=0x4e435232u;report.version=2;report.mode=g_near_clip_mode;
    report.frequency=sat_time_frequency_hz();report.command_cap=Cap;report.gouraud_cap=GCap;
    for(;;){
        const uint32_t step=report.count;
        const unsigned pose=(step<Steps?step:Steps-1)/2;
        sat_example_must(sat_app_frame_begin(SAT_RGB555(3,5,12),SAT_RGB555(3,5,12),nullptr));
        g_near_clip_ticks=0;g_near_clip_calls=0;g_near_clip_partial=0;g_near_clip_pieces=0;
        setup_pose(pose);
        const uint32_t start=now();
        draw_scene(pose);
        const uint32_t submission=now()-start;
        sat_vdp1_command_checkpoint_t ck{};
        sat_example_must(sat_vdp1_command_checkpoint(&ck));
        sat_example_must(ck.used<Cap&&ck.gouraud_tables<=GCap?SAT_OK:SAT_ERR_CAPACITY);
        auto* commands=reinterpret_cast<volatile uint16_t*>(saturn::hal::vdp1::frame_commands());
        // The current HAL uses 64 KiB command storage followed by the
        // Gouraud table area in the same bank. Copy only live words.
        auto* tables=commands+32768;
        Row row{};
        row.step=step;row.pose=pose;row.clip_ticks=g_near_clip_ticks;
        row.calls=g_near_clip_calls;row.multi_piece=g_near_clip_partial;
        row.pieces=g_near_clip_pieces;row.submit_ticks=submission;
        row.commands=ck.used+1;row.gouraud=ck.gouraud_tables;
        const uint32_t serial=sat_vdp1_list_serial();
        if(step<Steps){
            for(unsigned i=0;i<unsigned(ck.used)*16;++i)g_near_clip_commands[step][i]=commands[i];
            for(unsigned i=0;i<unsigned(ck.gouraud_tables)*4;++i)g_near_clip_gouraud[step][i]=tables[i];
        }
        const uint32_t end=now();
        sat_example_must(sat_app_frame_end());
        row.end_ticks=now()-end;
        row.serial=sat_vdp1_list_serial();
        sat_vdp1_frame_stats_t stats{};
        sat_example_must(sat_vdp1_frame_stats(&stats));
        row.draw_timeouts=stats.draw_timeouts;
        sat_example_must(row.serial==serial+1?SAT_OK:SAT_ERR_INVALID_ARG);
        if(step<Steps){
            // The final END command is added by submit. Read it from the
            // same published bank before the next begin_frame.
            for(unsigned i=unsigned(ck.used)*16;i<row.commands*16;++i)
                g_near_clip_commands[step][i]=commands[i];
            report.rows[step]=row;
            asm volatile("":::"memory");
            report.count=step+1;
            if(report.count==Steps){asm volatile("":::"memory");report.done=1;}
        }
    }
}
