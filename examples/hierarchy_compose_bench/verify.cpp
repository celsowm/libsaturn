#include <stdint.h>
#include "saturn/parallel.h"
#include "saturn/time.h"
#include "saturn/skeleton3d.h"
#include "src/graphics/3d/geometry/hierarchy_evaluate.hpp"
#include "src/graphics/3d/geometry/affine_compose_sh2.hpp"

#if !defined(__sh__)
#error Target benchmark requires native SH-2
#endif
namespace {
namespace h=saturn::core::hierarchy;
constexpr unsigned N=96, rounds=64, scenarios=10;
using Compose=sat_result_t(*)(sat_mat4_t*,const sat_mat4_t*,const sat_mat4_t*);
struct Row {uint32_t cases,errors,first_case,control_ticks,cpp_ticks,asm_ticks,nodes,compositions;};
struct alignas(16) Lane {uint32_t slave,sr,ccr,done,edge_checks,edge_errors,reserved[2];Row rows[scenarios];};
struct alignas(16) Report {uint32_t magic,version,done,init_status,slave_ready,parallel_status,frequency,reserved;Lane lanes[3];};
static_assert(sizeof(Row)==32&&sizeof(Lane)==352&&sizeof(Report)==1088);
struct State {sat_transform3d_node_t nodes[N];sat_transform3d_world_t world;uint16_t scratch[N];};
struct Workspace {State control,cpp,assembly;sat_mat4_t expected[N];};
static Workspace work[2];
static uint32_t rng(uint32_t& s){s=s*1664525u+1013904223u;return s;}
static int32_t sample(uint32_t& s,int32_t r){return int32_t((rng(s)>>8)%uint32_t(2*r+1))-r;}
static bool equal(const sat_mat4_t& a,const sat_mat4_t& b){
    for(unsigned j=0;j<16;++j)if(a.m[j]!=b.m[j])return false;return true;
}
static sat_mat4_t oracle(const sat_mat4_t& a,const sat_mat4_t& b){
    sat_mat4_t out{};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
        int64_t sum=0;
        for(unsigned k=0;k<4;++k){
            const int64_t p=int64_t(a.m[r*4+k])*b.m[k*4+c];
            sum+=p/65536-(p<0&&p%65536!=0);
        }
        out.m[r*4+c]=int32_t(sum);
    }
    return out;
}
static sat_mat4_t local(uint32_t& s,unsigned k,unsigned i,unsigned sc){
    // Bounded three-axis rotations with real Q15 quaternion conversion.
    // Rotation/translation chains stay in the fixed-point-safe domain.
    sat_quat16_t raw{int16_t(sample(s,12000)),int16_t(sample(s,12000)),int16_t(sample(s,12000)),32767},q{};
    sat_quat16_normalize(&q,&raw);
    const sat_vec3_t t{sample(s,65536),sample(s,65536),sample(s,65536)};
    sat_mat4_t m{};sat_quat16_to_mat4(&m,&q,&t);
    if(sc==4||sc==7){
        // Branching, not an exponentially scaling long chain.
        for(unsigned r=0;r<12;r+=4){
            m.m[r]=int32_t(int64_t(m.m[r])*98304>>16);
            m.m[r+1]=int32_t(int64_t(m.m[r+1])*-49152>>16);
            m.m[r+2]=int32_t(int64_t(m.m[r+2])*32768>>16);
        }
    }
    if(sc==5&&((k+i)%3==0)){sat_mat4_identity(&m);m.m[3]=t.x;m.m[7]=t.y;m.m[11]=t.z;}
    return m;
}
static void init(State& a,unsigned n){
    sat_transform3d_world_init(&a.world,a.nodes,N);
    for(unsigned i=0;i<n;++i){uint16_t id;sat_transform3d_create(&a.world,&id);}
}
static void copy_state(State& a,const State& b,unsigned n){
    for(unsigned i=0;i<n;++i)a.nodes[i]=b.nodes[i];
    a.world={a.nodes,uint16_t(n),uint16_t(N),b.world.pending};
}
static bool equivalent(const State& a,const State& b,unsigned n){
    if(a.world.pending!=b.world.pending)return false;
    for(unsigned i=0;i<n;++i){
        const auto& x=a.nodes[i];const auto& y=b.nodes[i];
        if(x.dirty!=y.dirty||x.eval_state!=y.eval_state||x.parent!=y.parent||
           !equal(x.world,y.world)||!equal(x.local_matrix,y.local_matrix))return false;
    }return true;
}
static void edge(Lane& l,Workspace& w){
    uint32_t seed=0x21538329;
    for(unsigned k=0;k<32;++k){
        sat_mat4_t a=local(seed,k,0,4),b=local(seed,k,1,4);
        if(k%4==0)b.m[12]=1024; // generic fallback
        const auto expected=oracle(a,b);
        const Compose fns[]={h::compose_cpp,h::compose_asm};
        for(auto fn:fns){
            for(unsigned alias=0;alias<3;++alias){
                sat_mat4_t aa=a,bb=b;
                struct Guard{uint32_t lo;sat_mat4_t out;uint32_t hi;} g{0x12345678,{},0x87654321};
                auto* dest=alias==1?&aa:alias==2?&bb:&g.out;
                const auto status=fn(dest,&aa,&bb);
                ++l.edge_checks;
                if(status!=SAT_OK||!equal(*dest,expected)||g.lo!=0x12345678||g.hi!=0x87654321)
                    ++l.edge_errors;
            }
        }
    }
    // Invalid parent and cycle must be rejected by the very same traversal.
    for(unsigned fault=0;fault<3;++fault){
        init(w.control,3);
        if(fault==0)w.control.nodes[1].parent=3;
        else if(fault==1){w.control.nodes[0].parent=1;w.control.nodes[1].parent=0;}
        copy_state(w.cpp,w.control,3);copy_state(w.assembly,w.control,3);
        const uint16_t cap=fault==2?2:3;
        auto x=sat_transform3d_evaluate(&w.control.world,w.control.scratch,cap);
        auto y=h::evaluate(&w.cpp.world,w.cpp.scratch,cap,h::compose_cpp);
        auto z=h::evaluate(&w.assembly.world,w.assembly.scratch,cap,h::compose_asm);
        ++l.edge_checks;
        if(x==SAT_OK||x!=y||x!=z)++l.edge_errors;
    }
}
static void scenario(Workspace& w,Row& r,unsigned sc,unsigned k){
    const unsigned sizes[]={1,22,66,96,66,66,66,66,66,66};
    const unsigned n=sizes[sc];uint32_t seed=0x3a51239u+0x9e3779b9u*k+sc;
    init(w.control,n);
    for(unsigned i=0;i<n;++i){
        uint16_t parent=SAT_TRANSFORM3D_ROOT;
        if(sc==6){if(i+1<n)parent=uint16_t(i+1);} // reverse order
        else if(i&&!(sc==5&&i%11==0))
            parent=uint16_t(sc==2||sc==3?i-1:(i-1)/3);
        sat_transform3d_set_parent(&w.control.world,uint16_t(i),parent);
        auto m=local(seed,k,i,sc);
        sat_transform3d_set_local_matrix(&w.control.world,uint16_t(i),&m);
    }
    if(sc==7){
        // Evaluate an earlier pose, then replace the entire pose before timing.
        sat_transform3d_evaluate(&w.control.world,w.control.scratch,N);
        for(unsigned i=0;i<n;++i){
            auto m=local(seed,k+1,i,sc);
            sat_transform3d_set_local_matrix(&w.control.world,uint16_t(i),&m);
        }
    }
    if(sc>=8){
        sat_transform3d_evaluate(&w.control.world,w.control.scratch,N);
        if(sc==9){
            auto m=local(seed,k,n-1,sc);
            sat_transform3d_set_local_matrix(&w.control.world,uint16_t(n-1),&m);
        }
    }
    copy_state(w.cpp,w.control,n);copy_state(w.assembly,w.control,n);
    // Independent order for oracle: reverse chain or topologically ordered forest.
    for(unsigned j=0;j<n;++j){
        const unsigned i=sc==6?n-1-j:j;
        const auto& node=w.control.nodes[i];
        w.expected[i]=node.parent==SAT_TRANSFORM3D_ROOT?node.local_matrix:
            oracle(w.expected[node.parent],node.local_matrix);
    }
    sat_result_t x=SAT_ERR_INVALID_ARG,y=x,z=x;
    auto a=[&]{auto t=sat_time_frc();x=sat_transform3d_evaluate(&w.control.world,w.control.scratch,N);
        r.control_ticks+=uint16_t(sat_time_frc()-t);};
    auto b=[&]{auto t=sat_time_frc();y=h::evaluate(&w.cpp.world,w.cpp.scratch,N,h::compose_cpp);
        r.cpp_ticks+=uint16_t(sat_time_frc()-t);};
    auto c=[&]{auto t=sat_time_frc();z=h::evaluate(&w.assembly.world,w.assembly.scratch,N,h::compose_asm);
        r.asm_ticks+=uint16_t(sat_time_frc()-t);};
    switch(k%6){case 0:a();b();c();break;case 1:c();b();a();break;case 2:b();c();a();break;
    case 3:a();c();b();break;case 4:c();a();b();break;default:b();a();c();break;}
    bool ok=x==SAT_OK&&x==y&&x==z&&equivalent(w.control,w.cpp,n)&&equivalent(w.control,w.assembly,n);
    unsigned mul=0;
    for(unsigned i=0;i<n;++i){
        const auto& node=w.control.nodes[i];
        ok=ok&&equal(node.world,w.expected[i]);
        if(sc!=8&&node.eval_state==2&&node.parent!=SAT_TRANSFORM3D_ROOT)++mul;
    }
    ++r.cases;r.nodes+=n;r.compositions+=mul;
    if(!ok){if(!r.errors)r.first_case=k;++r.errors;}
}
struct Context{Lane* lanes;unsigned round,sc;};
static void range_run(void* p,uint32_t first,uint32_t last){
    const auto& ctx=*static_cast<Context*>(p);
    for(unsigned i=first;i<last;++i){
        auto& l=ctx.lanes[i];
        if(ctx.round==0&&ctx.sc==0){
            l={};uint32_t sp,sr;asm volatile("mov r15,%0":"=r"(sp));asm volatile("stc sr,%0":"=r"(sr));
            l.slave=sp<0x06004000u;l.sr=sr;l.ccr=*reinterpret_cast<volatile uint8_t*>(0xfffffe92u);
            edge(l,work[i]);
        }
        scenario(work[i],l.rows[ctx.sc],ctx.sc,ctx.round);
        if(ctx.round+1==rounds&&ctx.sc+1==scenarios)l.done=1;
    }
}
}
extern "C" {Report g_hierarchy_compose_bench;}
extern "C" unsigned hierarchy_compose_verify(void){
    auto& r=g_hierarchy_compose_bench;r.magic=0x48434231u;r.version=1;r.frequency=sat_time_frequency_hz();
    for(unsigned k=0;k<rounds;++k)for(unsigned sc=0;sc<scenarios;++sc){
        Context ctx{r.lanes,k,sc};range_run(&ctx,0,1);
    }
    sat_parallel_config_t config{SAT_PARALLEL_SLAVE,nullptr,0,0,60000u};
    r.init_status=uint32_t(-sat_parallel_init(&config));r.slave_ready=sat_parallel_slave_available();
    if(!r.init_status&&r.slave_ready)for(unsigned k=0;k<rounds&&!r.parallel_status;++k)
        for(unsigned sc=0;sc<scenarios&&!r.parallel_status;++sc){
            Context ctx{r.lanes+1,k,sc};
            r.parallel_status=uint32_t(-sat_parallel_for(0,2,1,range_run,&ctx,r.lanes+1,2*sizeof(Lane)));
        }
    unsigned errors=r.init_status||!r.slave_ready||r.parallel_status;
    for(unsigned i=0;i<3;++i){auto& l=r.lanes[i];errors+=l.edge_errors+(!l.done)+(l.slave!=(i==2));
        for(auto& row:l.rows)errors+=row.errors+(row.cases!=rounds);}
    asm volatile("":::"memory");r.done=1;return errors;
}
