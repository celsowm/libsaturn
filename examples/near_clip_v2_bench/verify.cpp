#include <stdint.h>
#include "saturn/time.h"
#include "saturn/parallel.h"
#include "src/graphics/3d/rendering/logic.hpp"
namespace {
namespace r=saturn::core::render3d;
constexpr unsigned N=32,S=24,R=64;
struct Row{uint32_t cases,errors,first_error,quads,pieces,control_ticks,divu_ticks,axial_ticks;};
struct alignas(16) Lane{uint32_t slave,sr,ccr,done,reserved[4];Row rows[S];};
struct alignas(16) Report{uint32_t magic,version,done,init_status,slave_ready,parallel_status,frequency,reserved;Lane lanes[3];};
static_assert(sizeof(Lane)==800&&sizeof(Report)==2432);
struct Result{sat_quad3_t quads[6];uint8_t count;sat_result_t status;};
struct Work{sat_quad3_t input[N];Result out[3][N];sat_vec3_t eye,forward;int32_t near;};
static Work work[2];
uint32_t rng(uint32_t& s){s=s*1664525u+1013904223u;return s;}
int jitter(uint32_t& s,int range){return int((rng(s)>>16)%unsigned(2*range+1))-range;}
constexpr int32_t F=65536;
void seed_case(Work& w,unsigned sc,unsigned k){
    uint32_t seed=0x4719523bu+k*0x9e3779b9u+sc;
    w.eye={0,0,0};w.forward={0,0,F};w.near=F;
    if(sc==8)w.forward={F,0,0};
    if(sc==9)w.forward={0,0,-F};
    if(sc==10){w.eye={2*F,-F,3*F};w.forward={39322,0,52429};}
    if(sc==11){w.eye={-F,3*F,-2*F};w.forward={37837,37837,37837};}
    if(sc==14){w.eye={INT32_MIN,0,0};w.forward={1,0,65535};}
    if(sc==15){if(k&1)w.near=0;else w.forward={0,0,0};}
    if(sc==16)w.forward={0,-F,0};
    if(sc==17)w.forward={-F,0,0};
    if(sc==18)w.forward={0,0,F/2};
    if(sc==20||sc==21)w.forward={39322,0,52429};
    if(sc==22)w.forward={0,F,0};
    if(sc==23)w.eye={0,0,INT32_MIN};
    for(unsigned i=0;i<N;++i){
        auto& q=w.input[i];
        const int32_t cx=jitter(seed,2*F),cy=jitter(seed,2*F);
        const int32_t zlo=F/2+jitter(seed,F/8),zhi=3*F+jitter(seed,F/4);
        for(unsigned j=0;j<4;++j){
            int32_t x=cx+((j==1||j==2)?F:-F);
            int32_t y=cy+(j>=2?F:-F);
            bool inside=j>=2;
            if(sc==0)inside=true;
            if(sc==1)inside=false;
            if(sc==2)inside=j==0;
            if(sc==4)inside=j!=0;
            if(sc==5)inside=(j&1)!=0;
            int32_t z=inside?zhi:zlo;
            if(sc==6)z=j==0?F:j==1?F-1:F+1;
            if(sc==8)q.v[j]={z,y,x};
            else if(sc==9)q.v[j]={x,y,-z};
            else if(sc==10||sc==11)q.v[j]={x+w.eye.x,y+w.eye.y,z+w.eye.z};
            else if(sc==12)q.v[j]={j>=2?INT32_MAX-1024:INT32_MIN+1024,y,z};
            else if(sc==13)q.v[j]={x,y,j>=2?1500000000:-1500000000};
            else if(sc==14)q.v[j]={INT32_MAX-1024,y,j>=2?2*F:-2*F};
            else if(sc==16)q.v[j]={x,-z,y};
            else if(sc==17)q.v[j]={-z,y,x};
            else if(sc==18)q.v[j]={x,y,2*z};
            else if(sc==19)q.v[j]={x,y,j>=2?F+1:F-1};
            else if(sc==20)q.v[j]={x,y,8*F};
            else if(sc==21)q.v[j]={x,y,-8*F};
            else if(sc==22)q.v[j]={x,z,y};
            else if(sc==23)q.v[j]={x,y,INT32_MAX-1024};
            else q.v[j]={x,y,z};
        }
        if(sc==7)q.v[3]=q.v[2];
        if(i&1){auto p=q.v[1];q.v[1]=q.v[3];q.v[3]=p;}
    }
}
bool equal(const sat_quad3_t& a,const sat_quad3_t& b){
    for(unsigned j=0;j<4;++j)
        if(a.v[j].x!=b.v[j].x||a.v[j].y!=b.v[j].y||a.v[j].z!=b.v[j].z)return false;
    return true;
}
// Scalar test oracle: classify by a bit mask, then construct the output
// ring using explicit indexed edges in the required incoming-edge order.
// Every generated product and sum in the original signed-64 expressions fits.
int64_t depth(const sat_vec3_t& p,const Work& w){
    return ((int64_t(p.x)-w.eye.x)*w.forward.x+
            (int64_t(p.y)-w.eye.y)*w.forward.y+
            (int64_t(p.z)-w.eye.z)*w.forward.z)/F;
}
sat_vec3_t intersection(const sat_vec3_t& a,const sat_vec3_t& b,int64_t d0,int64_t d1,int32_t plane){
    auto axis=[&](int32_t x,int32_t y){
        return int32_t(int64_t(x)+(int64_t(y)-x)*(int64_t(plane)-d0)/(d1-d0));
    };
    return {axis(a.x,b.x),axis(a.y,b.y),axis(a.z,b.z)};
}
bool oracle(const Work& w,unsigned i,const Result& o){
    if(w.near<=0||(!w.forward.x&&!w.forward.y&&!w.forward.z))
        return o.status==SAT_ERR_INVALID_ARG&&o.count==0xa5;
    if(o.status!=SAT_OK)return false;
    const auto& q=w.input[i];
    int64_t d[4];unsigned mask=0;
    for(unsigned j=0;j<4;++j){d[j]=depth(q.v[j],w);if(d[j]>=w.near)mask|=1u<<j;}
    if(mask==15)return o.count==1&&equal(q,o.quads[1]);
    if(!mask)return o.count==0;
    sat_vec3_t ring[6];unsigned n=0;
    for(unsigned j=0;j<4;++j){
        const unsigned from=(j+3)%4;
        if(((mask>>from)^(mask>>j))&1u)
            ring[n++]=intersection(q.v[from],q.v[j],d[from],d[j],w.near);
        if(mask&(1u<<j))ring[n++]=q.v[j];
    }
    if(n<3)return o.count==0;
    if(o.count!=n-2)return false;
    for(unsigned j=0;j<n-2;++j){
        sat_quad3_t expected={{ring[0],ring[j+1],ring[j+2],ring[j+2]}};
        if(!equal(expected,o.quads[j+1]))return false;
    }
    return true;
}
// Kernel experiment IDs are not output-buffer indices. Keep the mapping
// explicit at compile time and reject any unsupported mode/slot.
template<unsigned M,unsigned Slot>void run_batch(Work& w,uint32_t& ticks){
    static_assert(Slot<3);
    static_assert((M==0&&Slot==0)||(M==3&&Slot==1)||(M==4&&Slot==2));
    auto t=sat_time_frc();
    for(unsigned i=0;i<N;++i){
        auto& result=w.out[Slot][i];
        result.status=r::clip_world_quad_near<M>(&w.input[i],&w.eye,&w.forward,w.near,result.quads+1,&result.count);
    }
    ticks+=uint16_t(sat_time_frc()-t);
}
void scenario(Work& w,Row& row,unsigned sc,unsigned k){
    seed_case(w,sc,k);
    sat_quad3_t guard{};for(auto& v:guard.v)v={123456789,-123456789,987654321};
    for(auto& batch:w.out)for(auto& o:batch){o.count=0xa5;o.status=SAT_ERR_INVALID_ARG;for(auto& q:o.quads)q=guard;}
    auto a=[&]{run_batch<0,0>(w,row.control_ticks);};
    auto b=[&]{run_batch<3,1>(w,row.divu_ticks);};
    auto c=[&]{run_batch<4,2>(w,row.axial_ticks);};
    switch(k%6){case 0:a();b();c();break;case 1:c();b();a();break;case 2:b();c();a();break;case 3:a();c();b();break;case 4:c();a();b();break;default:b();a();c();}
    bool ok=true;
    for(unsigned i=0;i<N;++i){
        const auto& ref=w.out[0][i];
        if(ref.status==SAT_OK){
            if(ref.count>4)ok=false;else row.pieces+=ref.count;
        }
        for(unsigned m=0;m<3;++m){
            const auto& q=w.out[m][i];
            ok=ok&&oracle(w,i,q)&&q.status==ref.status&&q.count==ref.count;
            ok=ok&&equal(q.quads[0],guard)&&equal(q.quads[5],guard);
            for(unsigned j=1;j<=4;++j)ok=ok&&equal(q.quads[j],ref.quads[j]);
            if(q.status==SAT_OK&&q.count<=4)
                for(unsigned j=q.count+1;j<=4;++j)ok=ok&&equal(q.quads[j],guard);
            else if(q.status==SAT_ERR_INVALID_ARG)
                for(unsigned j=1;j<=4;++j)ok=ok&&equal(q.quads[j],guard);
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
extern "C" {Report g_near_clip_v2_bench;}
extern "C" unsigned near_clip_v2_verify(void){
    auto& r=g_near_clip_v2_bench;r.magic=0x4e434232u;r.version=2;r.frequency=sat_time_frequency_hz();
    for(unsigned k=0;k<R;++k)for(unsigned sc=0;sc<S;++sc){Context c{r.lanes,k,sc};run(&c,0,1);}
    sat_parallel_config_t cfg{SAT_PARALLEL_SLAVE,nullptr,0,0,60000u};
    r.init_status=uint32_t(-sat_parallel_init(&cfg));r.slave_ready=sat_parallel_slave_available();
    if(!r.init_status&&r.slave_ready)for(unsigned k=0;k<R&&!r.parallel_status;++k)
        for(unsigned sc=0;sc<S&&!r.parallel_status;++sc){Context c{r.lanes+1,k,sc};r.parallel_status=uint32_t(-sat_parallel_for(0,2,1,run,&c,r.lanes+1,2*sizeof(Lane)));}
    unsigned errors=r.init_status||!r.slave_ready||r.parallel_status;
    for(unsigned i=0;i<3;++i){auto& l=r.lanes[i];errors+=(!l.done)+(l.slave!=(i==2));for(auto& row:l.rows)errors+=row.errors+(row.cases!=R);}
    asm volatile("":::"memory");r.done=1;return errors;
}
