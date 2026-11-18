#include <stdint.h>
#include "saturn/time.h"
#include "saturn/parallel.h"
#include "src/graphics/3d/rendering/logic.hpp"
namespace {
namespace r=saturn::core::render3d;
constexpr unsigned N=32,S=16,R=64;
struct Row{uint32_t cases,errors,first_error,quads,pieces,control_ticks,planes_ticks,bounded_ticks;};
struct alignas(16) Lane{uint32_t slave,sr,ccr,done,reserved[4];Row rows[S];};
struct alignas(16) Report{uint32_t magic,version,done,init_status,slave_ready,parallel_status,frequency,reserved;Lane lanes[3];};
static_assert(sizeof(Lane)==544&&sizeof(Report)==1664);
struct Result{sat_quad2_t quads[8];uint8_t count;sat_result_t status;};
struct Work{sat_quad2_t input[N];Result out[3][N];};
static Work work[2];
uint32_t rng(uint32_t& s){s=s*1664525u+1013904223u;return s;}
int jitter(uint32_t& s,int range){return int((rng(s)>>16)%unsigned(2*range+1))-range;}
void rect(sat_quad2_t& q,int x0,int y0,int x1,int y1){
    q.x[0]=q.x[3]=int16_t(x0);q.x[1]=q.x[2]=int16_t(x1);
    q.y[0]=q.y[1]=int16_t(y0);q.y[2]=q.y[3]=int16_t(y1);
}
void seed_case(Work& w,unsigned sc,unsigned k){
    uint32_t seed=0x4719523bu+k*0x9e3779b9u+sc;
    for(unsigned i=0;i<N;++i){
        auto& q=w.input[i];int dx=jitter(seed,20),dy=jitter(seed,14);
        switch(sc){
        case 0:rect(q,-80+dx,-60+dy,80+dx,60+dy);break;
        case 1:rect(q,-280+dx,-70+dy,-80+dx,70+dy);break;
        case 2:rect(q,80+dx,-70+dy,280+dx,70+dy);break;
        case 3:rect(q,-80+dx,-240+dy,80+dx,-60+dy);break;
        case 4:rect(q,-80+dx,60+dy,80+dx,240+dy);break;
        case 5:rect(q,-300+dx,-250+dy,60+dx,70+dy);break;
        case 6:rect(q,-2047,-2047,2047,2047);break;
        case 7:rect(q,300+dx,-70+dy,500+dx,70+dy);break;
        case 8:
            q.x[0]=int16_t(-240+dx);q.y[0]=int16_t(dy);
            q.x[1]=int16_t(dx);q.y[1]=int16_t(-170+dy);
            q.x[2]=int16_t(240+dx);q.y[2]=int16_t(dy);
            q.x[3]=int16_t(dx);q.y[3]=int16_t(170+dy);break;
        case 9:rect(q,-240+dx,-100+dy,40+dx,140+dy);q.x[3]=q.x[2];q.y[3]=q.y[2];break;
        case 10:rect(q,-240+dx,0,60+dx,0);break;
        case 11:rect(q,-32768,-32768,32767,32767);break;
        case 12:for(unsigned j=0;j<4;++j){q.x[j]=int16_t(rng(seed)>>16);q.y[j]=int16_t(rng(seed)>>16);}break;
        case 13:rect(q,-1024,-1024,1023,1023);q.x[1]=int16_t(1023+dx);break;
        case 14:rect(q,-160,-112,159,111);q.x[i%4]=int16_t(-160+dx);break;
        default:rect(q,-2,-2,2,2);break;
        }
        // Both windings, same ordering of vertices relative to each control.
        if(i&1){auto x=q.x[1],y=q.y[1];q.x[1]=q.x[3];q.y[1]=q.y[3];q.x[3]=x;q.y[3]=y;}
    }
}
bool equal(const sat_quad2_t& a,const sat_quad2_t& b){
    for(unsigned j=0;j<4;++j)if(a.x[j]!=b.x[j]||a.y[j]!=b.y[j])return false;
    return true;
}
template<unsigned M>void run_batch(Work& w,unsigned width,unsigned height,uint32_t& ticks){
    auto t=sat_time_frc();
    for(unsigned i=0;i<N;++i){
        auto& result=w.out[M][i];
        result.status=r::clip_quad_screen<M>(&w.input[i],uint16_t(width),uint16_t(height),result.quads+1,&result.count);
    }
    ticks+=uint16_t(sat_time_frc()-t);
}
void scenario(Work& w,Row& row,unsigned sc,unsigned k){
    seed_case(w,sc,k);
    const unsigned width=sc==13?2048:sc==14?321:sc==15?2:320;
    const unsigned height=sc==13?2048:sc==14?225:sc==15?2:224;
    sat_quad2_t guard{};for(unsigned j=0;j<4;++j){guard.x[j]=12345;guard.y[j]=-12345;}
    for(auto& batch:w.out)for(auto& o:batch){o.count=0xa5;o.status=SAT_ERR_INVALID_ARG;for(auto& q:o.quads)q=guard;}
    auto a=[&]{run_batch<0>(w,width,height,row.control_ticks);};
    auto b=[&]{run_batch<1>(w,width,height,row.planes_ticks);};
    auto c=[&]{run_batch<2>(w,width,height,row.bounded_ticks);};
    switch(k%6){case 0:a();b();c();break;case 1:c();b();a();break;case 2:b();c();a();break;case 3:a();c();b();break;case 4:c();a();b();break;default:b();a();c();}
    bool ok=true;
    for(unsigned i=0;i<N;++i){
        const auto& ref=w.out[0][i];
        if(ref.status==SAT_OK){
            if(ref.count>6)ok=false;else row.pieces+=ref.count;
        }
        for(unsigned m=0;m<3;++m){
            const auto& q=w.out[m][i];ok=ok&&q.status==ref.status&&q.count==ref.count;
            ok=ok&&equal(q.quads[0],guard)&&equal(q.quads[7],guard);
            for(unsigned j=1;j<=6;++j)ok=ok&&equal(q.quads[j],ref.quads[j]);
            if(q.status==SAT_OK&&q.count<=6)
                for(unsigned j=q.count+1;j<=6;++j)ok=ok&&equal(q.quads[j],guard);
        }
    }
    ++row.cases;row.quads+=N;if(!ok){if(!row.errors)row.first_error=k;++row.errors;}
}
struct Context{Lane* lanes;unsigned round,sc;};
void run(void* ptr,uint32_t first,uint32_t last){
    const auto& c=*static_cast<Context*>(ptr);
    for(unsigned i=first;i<last;++i){
        auto& l=c.lanes[i];if(!c.round&&!c.sc){l={};uint32_t sp,sr;
            asm volatile("mov r15,%0":"=r"(sp));asm volatile("stc sr,%0":"=r"(sr));
            l.slave=sp<0x06004000u;l.sr=sr;l.ccr=*reinterpret_cast<volatile uint8_t*>(0xfffffe92u);}
        scenario(work[i],l.rows[c.sc],c.sc,c.round);
        if(c.round+1==R&&c.sc+1==S)l.done=1;
    }
}
}
extern "C" {Report g_screen_clip_bench;}
extern "C" unsigned screen_clip_verify(void){
    auto& r=g_screen_clip_bench;r.magic=0x53434231u;r.version=1;r.frequency=sat_time_frequency_hz();
    for(unsigned k=0;k<R;++k)for(unsigned sc=0;sc<S;++sc){Context c{r.lanes,k,sc};run(&c,0,1);}
    sat_parallel_config_t cfg{SAT_PARALLEL_SLAVE,nullptr,0,0,60000u};
    r.init_status=uint32_t(-sat_parallel_init(&cfg));r.slave_ready=sat_parallel_slave_available();
    if(!r.init_status&&r.slave_ready)for(unsigned k=0;k<R&&!r.parallel_status;++k)
        for(unsigned sc=0;sc<S&&!r.parallel_status;++sc){Context c{r.lanes+1,k,sc};r.parallel_status=uint32_t(-sat_parallel_for(0,2,1,run,&c,r.lanes+1,2*sizeof(Lane)));}
    unsigned errors=r.init_status||!r.slave_ready||r.parallel_status;
    for(unsigned i=0;i<3;++i){auto& l=r.lanes[i];errors+=(!l.done)+(l.slave!=(i==2));for(auto& row:l.rows)errors+=row.errors+(row.cases!=R);}
    asm volatile("":::"memory");r.done=1;return errors;
}
