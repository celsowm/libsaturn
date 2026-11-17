/* Compare experimental native SH-2 projection against the actual optimized
 * batch control, V1 retry, V2 guarded suffix and an independent software oracle.
 * FRT sums exclude input setup, oracle, guards and verification.
 * Rotate all three timed implementations; no production dispatch changes. */
#include <stdint.h>
#include <stddef.h>
#include "saturn/parallel.h"
#include "saturn/time.h"
#include "src/graphics/3d/rendering/static_project_sh2.hpp"
#if !defined(__sh__)
#error Target diagnostic requires native SH-2
#endif
namespace {
namespace render=saturn::core::render3d;
constexpr unsigned N=256, rounds=128, scenarios=19;
struct Row {uint32_t cases,errors,first_case,asm_ticks,control_ticks,vertices,fast,guarded_ticks,guarded_fast;};
struct alignas(16) Lane {uint32_t slave,sr,ccr,done;Row rows[scenarios];};
struct alignas(16) Report {
    uint32_t magic,version,done,init_status,slave_ready,parallel_status,frequency,reserved;
    Lane lanes[3];
};
static_assert(sizeof(Row)==36 && sizeof(Lane)==704 && sizeof(Report)==2144);
struct Workspace {sat_vec3_t points[N];sat_projected_vertex_t a[N+2],b[N+2],c[N+2],d[N+2];uint32_t seed;};
static Workspace work[2];
static uint32_t rng(uint32_t& s){s=s*1664525u+1013904223u;return s;}
static int32_t sample(uint32_t& s,int32_t r){return int32_t((rng(s)>>8)%uint32_t(r*2+1))-r;}
static void fill(void* p,size_t n){
    auto* b=static_cast<uint8_t*>(p);while(n--)*b++=0xa5;
}
static bool same(const void* p,const void* q,size_t n){
    auto* a=static_cast<const uint8_t*>(p);auto* b=static_cast<const uint8_t*>(q);
    while(n--)if(*a++!=*b++)return false;return true;
}
static int32_t row(const sat_fx16_t* m,const sat_vec3_t& p){
    // Test ranges keep each product and row within int32_t.
    return int32_t((int64_t(m[0])*p.x)>>16)+int32_t((int64_t(m[1])*p.y)>>16)+
           int32_t((int64_t(m[2])*p.z)>>16)+m[3];
}
static int16_t axis(int32_t c,int32_t s,int32_t w){
    int64_t v=(int64_t(c)*s/w)/2;
    if(v>2047)v=2047;if(v<-2047)v=-2047;
    return int16_t(v);
}
static void oracle(const sat_fx16_t* m,const sat_vec3_t* p,unsigned n,
                   int16_t width,int16_t height,sat_projected_vertex_t* out){
    for(unsigned i=0;i<n;++i){
        const int32_t w=row(m+12,p[i]);out[i].w=w;
        if(w<=0)continue;
        out[i].x=axis(row(m,p[i]),width,w);
        out[i].y=axis(row(m+4,p[i]),-int32_t(height),w);
    }
}
static void scenario(Workspace& w,Row& r,unsigned sc,unsigned k,uint32_t& seed){
    // 0: empty, 1: singleton, 2: rotated/32, 3: dense/256,
    // 4: sparse/64, 5: behind, 6: X clamp, 7: Y clamp,
    // 8: large W prefix rejection, 9: mixed/odd and 640x448.
    // V2 adds threshold neighbours and deterministic prefix rejection.
    // 10..13: rotated/8,16,31,64; 14..18: first/middle/last,
    // alternating, late Y rejection (128 vertices each).
    const unsigned sizes[]={0,1,32,256,64,33,33,33,33,127,8,16,31,64,128,128,128,128,128};
    const unsigned n=sizes[sc];
    sat_mat4_t matrix{};
    auto* m=matrix.m;
    m[0]=65536;m[1]=sample(seed,17000);m[2]=sample(seed,17000);m[3]=sample(seed,32768);
    m[4]=sample(seed,17000);m[5]=65536;m[6]=sample(seed,17000);m[7]=sample(seed,32768);
    m[12]=sample(seed,8000);m[13]=sample(seed,8000);m[14]=sample(seed,8000);m[15]=4*65536;
    if(sc==4){m[1]=m[2]=m[4]=m[12]=0;}
    if(sc>=5&&sc<=9){m[12]=m[13]=0;m[14]=65536;m[15]=0;}
    for(unsigned i=0;i<n;++i){
        auto& p=w.points[i];p={sample(seed,131072),sample(seed,131072),sample(seed,65536)};
        if(sc>=5&&sc<=9)p.z=4*65536+sample(seed,32768);
        if(sc==8)p.z=100000000+sample(seed,65536);
        if(sc==5 && i%5==0)p.z=i%2?0:-65536;
        if(sc==6 && i%5==0){p.x=20000000;p.y=0;p.z=4096;}
        if(sc==7 && i%5==0){p.x=0;p.y=-20000000;p.z=4096;}
        if(sc==9 && i%7==0){p.z=i%2?1:-1;p.x=2000000;}
    }
    if(sc>=14){
        m[0]=m[5]=65536;m[1]=1;
        m[2]=m[3]=m[4]=m[6]=m[7]=0;
        m[12]=m[13]=0;m[14]=65536;m[15]=0;
        for(unsigned i=0;i<n;++i){
            auto& p=w.points[i];p.z=4*65536;
            const bool reject=(sc==14&&i==0)||(sc==15&&i==n/2)||
                (sc==16&&i==n-1)||(sc==17&&(i&1));
            if(reject)p.z=-65536;
            if(sc==18&&i==96){p.x=0;p.y=-20000000;p.z=4096;}
        }
    }
    if(sc==6||sc==7){m[1]=m[2]=m[3]=m[4]=m[6]=m[7]=0;m[0]=m[5]=65536;}
    // Force a rotated dispatch even in X/Y-clamp cases without changing W=Z:
    if(sc==6||sc==7)m[1]=1;
    const int16_t width=sc==9?640:320,height=sc==9?448:224;
    fill(w.a,sizeof(w.a));fill(w.b,sizeof(w.b));fill(w.c,sizeof(w.c));fill(w.d,sizeof(w.d));
    uint32_t fast=0,guarded=0;
    auto run_candidate=[&]{
        const uint16_t t=sat_time_frc();
        fast=render::static_project::candidate(m,w.points,uint16_t(n),width,height,w.a+1);
        r.asm_ticks+=uint16_t(sat_time_frc()-t);
    };
    auto run_control=[&]{
        const uint16_t t=sat_time_frc();
        render::project_vertices(m,w.points,uint16_t(n),width,height,w.b+1);
        r.control_ticks+=uint16_t(sat_time_frc()-t);
    };
    auto run_guarded=[&]{
        const uint16_t t=sat_time_frc();
        guarded=render::static_project::candidate_guarded(m,w.points,uint16_t(n),width,height,w.d+1);
        r.guarded_ticks+=uint16_t(sat_time_frc()-t);
    };
    // All six permutations; alternate the two remaining rounds.
    switch(k%6){
        case 0:run_control();run_candidate();run_guarded();break;
        case 1:run_guarded();run_candidate();run_control();break;
        case 2:run_candidate();run_guarded();run_control();break;
        case 3:run_control();run_guarded();run_candidate();break;
        case 4:run_guarded();run_control();run_candidate();break;
        default:run_candidate();run_control();run_guarded();break;
    }
    oracle(m,w.points,n,width,height,w.c+1);
    ++r.cases;r.vertices+=n;r.fast+=fast;r.guarded_fast+=guarded;
    if(fast>n||guarded>n||!same(w.d,w.c,sizeof(w.d))||!same(w.a,w.c,sizeof(w.a))||!same(w.b,w.c,sizeof(w.b))){
        if(!r.errors)r.first_case=k;
        ++r.errors;
    }
}
static void run_group(Lane& l,Workspace& w,unsigned k,unsigned first,unsigned last){
    if(first==0){
        if(k==0){
            l={};
            uint32_t sp,sr;asm volatile("mov r15,%0":"=r"(sp));asm volatile("stc sr,%0":"=r"(sr));
            l.slave=sp<0x06004000u;l.sr=sr;l.ccr=*reinterpret_cast<volatile uint8_t*>(0xfffffe92u);
        }
        // Keep V1's original first ten input streams bit-identical.
        w.seed=0x8ca7130bu+0x9e3779b9u*k;
    }
    for(unsigned sc=first;sc<last;++sc)scenario(w,l.rows[sc],sc,k,w.seed);
    if(k+1==rounds&&last==scenarios)l.done=1;
}
struct RoundContext {Lane* lanes;unsigned round,first,last;};
static void range_run(void* ctx,uint32_t first,uint32_t last){
    const auto& job=*static_cast<RoundContext*>(ctx);
    for(unsigned i=first;i<last;++i)run_group(job.lanes[i],work[i],job.round,job.first,job.last);
}
}
extern "C" {Report g_static_project_bench;}
extern "C" unsigned static_project_verify(void){
    auto& r=g_static_project_bench;
    r.magic=0x53504231u;r.version=2;r.frequency=sat_time_frequency_hz();
    for(unsigned k=0;k<rounds;++k)run_group(r.lanes[0],work[0],k,0,scenarios);
    sat_parallel_config_t config{SAT_PARALLEL_SLAVE,nullptr,0,0,60000u};
    r.init_status=uint32_t(-sat_parallel_init(&config));
    r.slave_ready=sat_parallel_slave_available();
    // Keep each task well within the range runtime's 60,000 FRT wait budget.
    if(!r.init_status&&r.slave_ready){
        for(unsigned k=0;k<rounds;++k){
            // Split larger workload to keep each dispatch below the runtime FRT timeout.
            for(unsigned first=0;first<scenarios;first+=3){
                const unsigned last=first+3<scenarios?first+3:scenarios;
                RoundContext context{r.lanes+1,k,first,last};
                r.parallel_status=uint32_t(-sat_parallel_for(0,2,1,range_run,&context,r.lanes+1,2*sizeof(Lane)));
                if(r.parallel_status)break;
            }
            if(r.parallel_status)break;
        }
    }
    unsigned errors=r.init_status||!r.slave_ready||r.parallel_status;
    for(unsigned i=0;i<3;++i){
        auto& l=r.lanes[i];
        if(!l.done||l.slave!=(i==2)||(l.sr&2u))++errors;
        for(auto& row:l.rows)errors+=row.errors+(row.cases!=rounds);
    }
    asm volatile("" ::: "memory");r.done=1;
    return errors;
}
