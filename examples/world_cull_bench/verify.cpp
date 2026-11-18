#include <stdint.h>
#include "saturn/time.h"
#include "saturn/parallel.h"
#include "src/core/geometry/mesh_logic.hpp"
namespace {
namespace m=saturn::core::mesh3d;
constexpr unsigned N=32,S=24,R=64;
struct Row{uint32_t cases,errors,first_error,faces,visible,control_ticks,axis_ticks,split_ticks;};
struct alignas(16) Lane{uint32_t slave,sr,ccr,done,reserved[4];Row rows[S];};
struct alignas(16) Report{uint32_t magic,version,done,init_status,slave_ready,parallel_status,frequency,reserved;Lane lanes[3];};
static_assert(sizeof(Lane)==800&&sizeof(Report)==2432);
struct Work{
    sat_quad3_t input[N];sat_vec3_t eye[N];
    uint8_t out[3][N+2];
};
static Work work[2];
uint32_t rng(uint32_t& s){s=s*1664525u+1013904223u;return s;}
int32_t jitter(uint32_t& s,unsigned n){return int32_t((rng(s)>>8)%(2*n+1))-int32_t(n);}
sat_vec3_t sum(sat_vec3_t a,sat_vec3_t b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
sat_vec3_t center(const sat_quad3_t& q){
    return {int32_t((int64_t(q.v[0].x)+q.v[1].x+q.v[2].x+q.v[3].x)/4),
            int32_t((int64_t(q.v[0].y)+q.v[1].y+q.v[2].y+q.v[3].y)/4),
            int32_t((int64_t(q.v[0].z)+q.v[1].z+q.v[2].z+q.v[3].z)/4)};
}
void seed_case(Work& w,unsigned sc,unsigned k){
    uint32_t seed=0x8ccb2d41u+k*0x9e3779b9u+sc;
    for(unsigned i=0;i<N;++i){
        const unsigned type=sc==20?(i%2?3:12):sc;
        sat_vec3_t a={jitter(seed,500000),jitter(seed,500000),jitter(seed,500000)};
        const int32_t e=1024+int32_t(rng(seed)%13000);
        sat_vec3_t b={e,0,0},d={0,e,0},warp={0,0,0};
        switch(type){
            case 1:b={0,e,0};d={0,0,e};break;
            case 2:b={0,0,e};d={e,0,0};break;
            case 3:case 4:case 13:case 14:case 15:case 19:case 21:case 23:
                b={e,e/3+3,17};d={-10,e,e/2+1};break;
            case 8:b={32767,-32768,32767};d={-32768,32767,32767};break;
            case 9:b={32768,-32769,10};d={0,65537,120};break;
            case 10:b={65536,0,0};d={0,4*65536,0};break;
            case 11:b={65536,131072,196608};d={-131072,65536,262144};break;
            case 12:b={65537,131071,196605};d={-131075,65538,262147};break;
            case 16:b={1<<26,0,0};d={0,1<<26,1<<25};break;
            case 17:b={1<<25,0,0};d={0,1<<25,0};break;
            case 18:b={(1<<25)-1,0,0};d={0,1<<25,0};break;
            case 22:b={65536,131072,196608};d={131072,-65536,-262144};break;
            default:break;
        }
        if(type==14)a={700000000,-700000000,700000000};
        if(type==15)a={-123457,-234561,-345679};
        if(type==19||type==23)warp={-11,9,13};
        if(type==7)d=b;
        sat_quad3_t q={{a,sum(a,b),sum(sum(sum(a,b),d),warp),sum(a,d)}};
        if(type==5)q.v[1]=a;
        if(type==6)q.v[3]=a;
        if(type==4||(i&1)){auto v=q.v[1];q.v[1]=q.v[3];q.v[3]=v;}
        const sat_vec3_t c=center(q);
        sat_vec3_t v={jitter(seed,1000000),jitter(seed,1000000),jitter(seed,1000000)};
        if(i%8==0)v={0,0,0};
        if(type==13){v=b;if(i%3==1)++v.z;else if(i%3==2)--v.z;}
        if(type==21)v={i&1?INT32_MIN:INT32_MAX,i&2?INT32_MIN:INT32_MAX,i&4?INT32_MIN:INT32_MAX};
        w.input[i]=q;
        if(type==21){
            // Shift centroid to exactly zero to make extreme view offsets
            // legal in the original signed subtraction.
            for(auto& p:w.input[i].v){p.x-=c.x;p.y-=c.y;p.z-=c.z;}
            // Truncating average can differ by one after translation.
            const auto recentered=center(w.input[i]);
            w.eye[i]={v.x==INT32_MIN?INT32_MIN+2:INT32_MAX-2,
                      v.y==INT32_MIN?INT32_MIN+2:INT32_MAX-2,
                      v.z==INT32_MIN?INT32_MIN+2:INT32_MAX-2};
            (void)recentered;
        }else w.eye[i]=sum(c,v);
    }
}
// Independent arithmetic oracle for the exact regime: split by 2^26
// using signed division and remainder (not the kernel's floor/mask split).
// Reduced high and low dot products both fit int64. A nonzero integer
// quotient dominates the final signed fractional remainder.
bool oracle(const sat_quad3_t& q,const sat_vec3_t& eye){
    auto cross=[&](unsigned a,unsigned b,int64_t (&n)[3]){
        const auto& o=q.v[0];const auto& x=q.v[a];const auto& y=q.v[b];
        int64_t ax=int64_t(x.x)-o.x,ay=int64_t(x.y)-o.y,az=int64_t(x.z)-o.z;
        int64_t bx=int64_t(y.x)-o.x,by=int64_t(y.y)-o.y,bz=int64_t(y.z)-o.z;
        n[0]=ay*bz-az*by;n[1]=az*bx-ax*bz;n[2]=ax*by-ay*bx;
    };
    int64_t n[3];cross(3,1,n);
    if(!(n[0]|n[1]|n[2]))cross(3,2,n);
    if(!(n[0]|n[1]|n[2]))cross(2,1,n);
    const auto c=center(q);
    const int32_t v[3]={eye.x-c.x,eye.y-c.y,eye.z-c.z};
    constexpr int64_t limit=int64_t(1)<<50,scale=int64_t(1)<<26;
    for(auto x:n)if(x>=limit||x<=-limit){
        // Compatibility oracle for the historical scaled regime: its
        // rounding is intentional, not replaced by exact raw dot.
        const auto scaled=m::quad_normal_scaled(q);
        return int64_t(scaled.x)*v[0]+int64_t(scaled.y)*v[1]+int64_t(scaled.z)*v[2]>0;
    }
    int64_t hi=0,lo=0;
    for(unsigned j=0;j<3;++j){hi+=(n[j]/scale)*v[j];lo+=(n[j]%scale)*v[j];}
    hi+=lo/scale;
    return hi>0||(hi==0&&lo%scale>0);
}
template<unsigned M,unsigned Slot>void run_batch(Work& w,uint32_t& ticks){
    static_assert(M==Slot&&Slot<3);
    auto t=sat_time_frc();
    for(unsigned i=0;i<N;++i)
        w.out[Slot][i+1]=uint8_t(m::quad_visible<M>(w.input[i],w.eye[i]));
    ticks+=uint16_t(sat_time_frc()-t);
}
void scenario(Work& w,Row& row,unsigned sc,unsigned k){
    seed_case(w,sc,k);
    for(auto& batch:w.out)for(auto& o:batch)o=0xa5;
    auto a=[&]{run_batch<0,0>(w,row.control_ticks);};
    auto b=[&]{run_batch<1,1>(w,row.axis_ticks);};
    auto c=[&]{run_batch<2,2>(w,row.split_ticks);};
    switch(k%6){case 0:a();b();c();break;case 1:c();b();a();break;case 2:b();c();a();break;case 3:a();c();b();break;case 4:c();a();b();break;default:b();a();c();}
    bool ok=true;
    for(unsigned i=0;i<N;++i){
        const uint8_t expected=uint8_t(oracle(w.input[i],w.eye[i]));
        row.visible+=expected;
        for(unsigned j=0;j<3;++j)
            ok=ok&&w.out[j][i+1]==expected&&w.out[j][0]==0xa5&&w.out[j][N+1]==0xa5;
    }
    ++row.cases;row.faces+=N;
    if(!ok){if(!row.errors)row.first_error=k;++row.errors;}
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
extern "C" {Report g_world_cull_bench;}
extern "C" unsigned world_cull_verify(void){
    auto& r=g_world_cull_bench;r.magic=0x57434231u;r.version=1;r.frequency=sat_time_frequency_hz();
    for(unsigned k=0;k<R;++k)for(unsigned sc=0;sc<S;++sc){Context c{r.lanes,k,sc};run(&c,0,1);}
    sat_parallel_config_t cfg{SAT_PARALLEL_SLAVE,nullptr,0,0,60000u};
    r.init_status=uint32_t(-sat_parallel_init(&cfg));r.slave_ready=sat_parallel_slave_available();
    if(!r.init_status&&r.slave_ready)for(unsigned k=0;k<R&&!r.parallel_status;++k)
        for(unsigned sc=0;sc<S&&!r.parallel_status;++sc){Context c{r.lanes+1,k,sc};r.parallel_status=uint32_t(-sat_parallel_for(0,2,1,run,&c,r.lanes+1,2*sizeof(Lane)));}
    unsigned errors=r.init_status||!r.slave_ready||r.parallel_status;
    for(unsigned i=0;i<3;++i){auto& l=r.lanes[i];errors+=(!l.done)+(l.slave!=(i==2));for(auto& row:l.rows)errors+=row.errors+(row.cases!=R);}
    asm volatile("":::"memory");r.done=1;return errors;
}
