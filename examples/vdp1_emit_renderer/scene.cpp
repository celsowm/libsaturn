#include <stdint.h>
#include "saturn/saturn.h"
#include "saturn/app.h"
#include "saturn/time.h"
#include "saturn/color.h"
#include "example_util.h"
#include "src/hal/vdp1/vdp1.hpp"
#include "src/core/runtime/logic.hpp"

extern "C" {extern const uint32_t g_vdp1_emit_mode;}
namespace {
namespace v=saturn::hal::vdp1;
namespace c=saturn::core;
constexpr unsigned Steps=24,Cap=128,GCap=32,N=96;
struct Row {
    uint32_t step,phase,pose,emit_ticks,calls,setup,capacity_reject,invalid_reject,
        commands,gouraud,end_ticks,draw_timeouts,serial;
};
struct Report {
    uint32_t magic,version,mode,frequency,count,done,command_cap,gouraud_cap;
    Row rows[Steps];
};
static_assert(sizeof(Report)==1280);
static_assert(sizeof(v::Command)==32);
struct Input {
    v::SpriteRequest sprite;
    v::ScaledSpriteRequest scaled;
    v::DistortedSpriteRequest distorted;
    v::PolygonRequest polygon;
    v::LineRequest line;
    sat_quad2_t quad;
    uint16_t prefix[6];
};
static Input inputs[N];
static sat_vdp1_texture_t tex;
static uint8_t pixels[16*16];
static uint16_t palette[256];
constexpr uint16_t shades[4]={SAT_RGB555(10,12,16),SAT_RGB555(18,20,16),
                             SAT_RGB555(18,16,20),SAT_RGB555(12,14,18)};
uint32_t now(){return uint32_t(sat_time_ticks());}
void seed(unsigned pose){
    for(unsigned i=0;i<N;++i){
        auto& a=inputs[i];
        const int16_t x=int16_t(-156+int(i%12)*26+int(pose)*2);
        const int16_t y=int16_t(-108+int(i/12)*26);
        const int16_t r=int16_t(x+16+int(i%3));
        const int16_t b=int16_t(y+16);
        const uint16_t flags=uint16_t((i&1?SAT_SPRITE_FLAG_FLIP_H:0)|
            (i&2?SAT_SPRITE_FLAG_FLIP_V:0)|(i&4?SAT_SPRITE_FLAG_OPAQUE:0)|
            (i&8?SAT_SPRITE_FLAG_MESH:0)|(i&16?SAT_SPRITE_FLAG_USER_CLIP:0));
        const bool clip=(flags&SAT_SPRITE_FLAG_USER_CLIP)!=0;
        a.sprite={x,y,tex.width,tex.height,tex.srca,tex.palette,flags,tex.format,clip};
        a.scaled={x,y,r,b,tex.width,tex.height,tex.srca,tex.palette,flags,tex.format,clip};
        a.distorted={{x,r,int16_t(r-2),int16_t(x+2)},{y,int16_t(y+2),b,int16_t(b-1)},
            tex.width,tex.height,tex.srca,tex.palette,flags,tex.format,clip};
        for(unsigned j=0;j<4;++j){a.quad.x[j]=a.distorted.x[j];a.quad.y[j]=a.distorted.y[j];}
        const uint16_t pf=i&4?SAT_SPRITE_FLAG_MESH:0;
        const uint16_t rgb=SAT_RGB555(8+i%12,12+i%10,20-i%6);
        a.polygon={x,y,r,int16_t(y+2),int16_t(r-2),b,int16_t(x+2),int16_t(b-1),rgb,pf,clip};
        a.line={x,y,r,b,rgb,pf,clip};
        a.prefix[0]=uint16_t(c::compose_polygon_ctrl(c::kVdp1CmdDistortedSprite,false)|c::compose_sprite_dir(flags));
        a.prefix[1]=0;
        a.prefix[2]=c::compose_inside_user_clip_pmod(c::compose_sprite_pmod(flags,tex.format),clip);
        a.prefix[3]=c::compose_sprite_colr(tex.palette,tex.format);
        a.prefix[4]=tex.srca;a.prefix[5]=uint16_t(((tex.width/8u)<<8u)|tex.height);
    }
}
sat_result_t emit(unsigned phase,unsigned i){
    const auto& a=inputs[i];
    switch(phase){
        case 0:return v::push_sprite(a.sprite);
        case 1:return v::push_scaled_sprite(a.scaled);
        case 2:return v::push_distorted_sprite(a.distorted);
        case 3:return v::push_projected_sprite(a.quad,tex,a.distorted.flags);
        case 4:return v::push_projected_program(a.quad,a.prefix,tex);
        default:
            switch(i%4){
                case 0:return v::push_polygon(a.polygon);
                case 1:return v::push_polyline(a.polygon);
                case 2:return v::push_line(a.line);
                default:return v::push_polygon_gouraud(a.polygon,shades);
            }
    }
}
void require(bool ok){sat_example_must(ok?SAT_OK:SAT_ERR_VERIFY_FAILED);}
}
extern "C" {
Report g_vdp1_emit_renderer;
uint16_t g_vdp1_emit_commands[Steps][Cap*16];
uint16_t g_vdp1_emit_gouraud[Steps][GCap*4];
}
extern "C" void vdp1_emit_renderer(void){
    sat_example_must(sat_app_init_default());
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x)
        pixels[y*16+x]=(x==0||y==0)?0:1+((x^y)&1);
    palette[1]=SAT_RGB555(8,22,25);palette[2]=SAT_RGB555(25,12,8);
    sat_example_must(sat_tex_upload_indexed8(&tex,pixels,16,16,palette,3));
    auto& report=g_vdp1_emit_renderer;
    report.magic=0x56455231u;report.version=1;report.mode=g_vdp1_emit_mode;
    report.frequency=sat_time_frequency_hz();report.command_cap=Cap;report.gouraud_cap=GCap;
    for(;;){
        const unsigned step=report.count,index=step<Steps?step:Steps-1;
        const unsigned phase=index/4,pose=(index/2)%2;
        seed(pose);
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,nullptr));
        sat_example_must(v::push_user_clip({12,8,307,215}));
        uint16_t used,capacity,reserved;bool overlay;
        v::command_stats(used,capacity,reserved,overlay);
        require(!overlay&&capacity>unsigned(used)+N+2);
        sat_example_must(v::reserve_overlay_commands(uint16_t(capacity-used-N-1)));
        Row row{};row.step=index;row.phase=phase;row.pose=pose;row.setup=used;
        const uint32_t stamp=now();
        for(unsigned i=0;i<N;++i)sat_example_must(emit(phase,i));
        row.emit_ticks=now()-stamp;row.calls=N;
        sat_vdp1_command_checkpoint_t full{};
        sat_example_must(v::command_checkpoint(&full));
        require(full.used==used+N&&full.gouraud_tables==(phase==5?N/4:0));
        auto* words=reinterpret_cast<volatile uint16_t*>(v::frame_commands());
        auto* tables=words+32768;
        // Reserved overlay slot remains untouched by both failing calls.
        uint16_t guard[16];
        for(unsigned i=0;i<16;++i)guard[i]=words[full.used*16u+i];
        auto invalid=inputs[0].sprite;invalid.width=0;
        row.invalid_reject=v::push_sprite(invalid)==SAT_ERR_INVALID_ARG;
        row.capacity_reject=v::push_sprite(inputs[0].sprite)==SAT_ERR_CAPACITY;
        require(row.invalid_reject&&row.capacity_reject);
        for(unsigned i=0;i<16;++i)require(guard[i]==words[full.used*16u+i]);
        sat_vdp1_command_checkpoint_t after{};
        sat_example_must(v::command_checkpoint(&after));
        require(after.used==full.used&&after.gouraud_tables==full.gouraud_tables);
        sat_example_must(v::begin_overlay_pass());
        const v::PolygonRequest swatch={-156,94,-136,94,-136,108,-156,108,SAT_RGB555(16,16,16),0,false};
        sat_example_must(v::push_polygon_gouraud(swatch,shades));
        sat_vdp1_command_checkpoint_t ck{};
        sat_example_must(v::command_checkpoint(&ck));
        require(ck.used==full.used+1&&ck.used<Cap&&ck.gouraud_tables==full.gouraud_tables+1&&ck.gouraud_tables<=GCap);
        row.commands=ck.used+1;row.gouraud=ck.gouraud_tables;
        if(step<Steps){
            for(unsigned i=0;i<unsigned(ck.used)*16;++i)g_vdp1_emit_commands[step][i]=words[i];
            for(unsigned i=0;i<unsigned(ck.gouraud_tables)*4;++i)g_vdp1_emit_gouraud[step][i]=tables[i];
        }
        const uint32_t serial=sat_vdp1_list_serial(),end=now();
        sat_example_must(sat_app_frame_end());
        row.end_ticks=now()-end;row.serial=sat_vdp1_list_serial();
        require(row.serial==serial+1);
        sat_vdp1_frame_stats_t stats{};
        sat_example_must(sat_vdp1_frame_stats(&stats));row.draw_timeouts=stats.draw_timeouts;
        if(step<Steps){
            for(unsigned i=unsigned(ck.used)*16;i<row.commands*16;++i)g_vdp1_emit_commands[step][i]=words[i];
            report.rows[step]=row;asm volatile("":::"memory");report.count=step+1;
            if(report.count==Steps){asm volatile("":::"memory");report.done=1;}
        }
    }
}
