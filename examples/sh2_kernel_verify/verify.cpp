// Diagnostic ELF only. Actual SH-2 routines vs reference arithmetic.
#include <stdint.h>
#include <stddef.h>
#include "saturn/parallel.h"
#include "saturn/time.h"
#include "src/graphics/3d/animation/rigid_project_sh2.hpp"
#include "src/graphics/3d/animation/blend_project_sh2.hpp"
#include "src/graphics/3d/animation/clip_joints_sh2.hpp"
#include "src/graphics/3d/rendering/paint_keys_sh2.hpp"
#include "src/graphics/3d/scene/projected_bounds_sh2.hpp"
#include "src/graphics/3d/scene/screen_cull_sh2.hpp"
#include "src/graphics/3d/scene/program_run_sh2.hpp"
#include "src/audio/music/codecs/adx.hpp"

#if !defined(__sh__)
#error This diagnostic must execute the actual SH-2 kernels.
#endif

namespace {
namespace rigid = saturn::skin3d::rigid_project;
namespace blend = saturn::skin3d::blend_project;
namespace clip = saturn::skin3d::clip_joints;
namespace paint = saturn::core::render3d::paint_keys;
namespace bounds = saturn::scene3d::projected_bounds;
namespace cull = saturn::scene3d::screen_cull;
namespace program = saturn::scene3d::program_run;

constexpr unsigned N=32, rounds=128, kernels=10;
enum Id { Rigid, Blend, Clip, Range, Spread, Quantise, Bounds, Cull, Program, Adx };
struct Row { uint32_t cases, errors, first_case, asm_ticks, reference_ticks; };
struct alignas(16) Lane {
    uint32_t on_slave, sr, ccr, done;
    Row rows[kernels];
};
struct alignas(16) Report {
    uint32_t magic, version, done, init_status, slave_ready, parallel_status, frequency, reserved;
    Lane lanes[3]; // serial master, parallel master, parallel slave
};
static_assert(sizeof(Row)==20 && sizeof(Lane)==224 && sizeof(Report)==704);
struct Workspace {
    sat_mat4_t joints[4], ca[5], cb[5];
    sat_skin_rigid_t rigid_records[N];
    sat_skin_blend_t blend_records[N];
    sat_projected_vertex_t a[N+2], b[N+2];
    uint32_t ka[N+2], kb[N+2];
    uint16_t ha[66], hb[66];
    uint16_t offsets[N*4], materials[N], order[N], indirect[N];
    uint8_t two_sided[N];
    sat_scene3d_material_program_t programs[2];
    sat_scene3d_projected_face_t ea[N+2], eb[N+2];
    uint32_t commands_a[N*8+2], commands_b[N*8+2];
};
static Workspace work[2]; // independent per CPU; no large worker stack.
static uint32_t rng(uint32_t& s) { s=s*1664525u+1013904223u;return s; }
static int32_t sample(uint32_t& s,int32_t r) { return int32_t((rng(s)>>16)%uint32_t(2*r+1))-r; }
static void fill(void* p,size_t n,uint8_t v=0xa5) {
    auto* b=static_cast<uint8_t*>(p);while(n--) *b++=v;
}
static bool same(const void* p,const void* q,size_t n) {
    auto* a=static_cast<const uint8_t*>(p);auto* b=static_cast<const uint8_t*>(q);
    while(n--) { if(*a++!=*b++)return false; }return true;
}
static bool guard(const void* p,size_t n) {
    auto* b=static_cast<const uint8_t*>(p);while(n--)if(*b++!=0xa5)return false;return true;
}
template<class F> static uint32_t timed(uint32_t& sum,F fn) {
    uint16_t t=sat_time_frc();uint32_t v=fn();sum+=uint16_t(sat_time_frc()-t);return v;
}
static void record(Row& r,unsigned case_no,bool ok) {
    ++r.cases;if(!ok){if(!r.errors)r.first_case=case_no;++r.errors;}
}
// Independent quotient oracle, not the reference header's target DIVU path.
static int16_t axis(int32_t c,int32_t scale,int32_t w) {
    return int16_t((int64_t(c)*scale/w)/2);
}
static bool inside(int32_t x,int32_t y,int32_t w,uint32_t mult,uint32_t last) {
    return w>0 && uint32_t(w)<=last && int64_t(x)>-int64_t(w)*mult &&
         int64_t(x)<int64_t(w)*mult && int64_t(y)>-int64_t(w)*mult && int64_t(y)<int64_t(w)*mult;
}
static int32_t row(const sat_fx16_t* m,const int16_t* p) {
    return int32_t((int64_t(m[0])*p[0]*2+int64_t(m[1])*p[1]*2+
                    int64_t(m[2])*p[2]*2+32768)>>16)+m[3];
}
static uint32_t rigid_oracle(const rigid::Job& j) {
    for(unsigned i=0;i<j.count;++i) {
        const auto& r=j.rigid[i];const auto* m=j.joints[r.joint].m;
        const auto x=row(m,r.pos),y=row(m+4,r.pos),w=row(m+8,r.pos);
        if(!inside(x,y,w,j.mult,j.w_last))return i;
        j.out[i]={axis(x,j.width,w),axis(y,j.y_scale,w),w};
    }return j.count;
}
static uint32_t blend_oracle(const blend::Job& j) {
    for(unsigned i=0;i<j.count;++i) {
        const auto& r=j.blend[i];if(r.count!=2||r.weight[0]!=16384)return i;
        const auto* m=j.joints[r.joint[0]].m;const auto* n=j.joints[r.joint[1]].m;
        int32_t v[3];for(unsigned k=0;k<3;++k)
            v[k]=int32_t((int64_t(row(m+4*k,r.pos[0]))+row(n+4*k,r.pos[1]))>>1);
        if(!inside(v[0],v[1],v[2],j.mult,j.w_last))return i;
        j.out[i]={axis(v[0],j.width,v[2]),axis(v[1],j.y_scale,v[2]),v[2]};
    }return j.count;
}
static bool projected_equal(Workspace& w,unsigned n,unsigned a,unsigned b) {
    // Failing vertex may be partly written by contract; everything after it must be untouched.
    unsigned written=a<n?a+1:a;
    return a==b && a<=n && same(w.a+1,w.b+1,a*sizeof(w.a[0])) &&
        guard(w.a,sizeof(w.a[0])) && guard(w.a+1+written,(N+1-written)*sizeof(w.a[0]));
}
static void projections(Workspace& w, Lane& l,uint32_t& s,unsigned k,unsigned n) {
    fill(w.joints,sizeof(w.joints),0);
    for(auto& m:w.joints) {
        for(unsigned i=0;i<12;++i)m.m[i]=sample(s,50000);
        m.m[3]=sample(s,20000);m.m[7]=sample(s,20000);m.m[11]=65536+sample(s,8000);
        m.m[8]=m.m[9]=m.m[10]=0;
    }
    for(unsigned i=0;i<n;++i) {
        auto& a=w.rigid_records[i];a.joint=uint8_t(i%3);a.reserved=0;
        for(auto& p:a.pos)p=int16_t(sample(s,30000));
        auto& b=w.blend_records[i];b={};b.count=2;b.weight[0]=16384;
        b.joint[0]=a.joint;b.joint[1]=uint8_t((i+1)%3);
        for(auto& pos:b.pos)for(auto& p:pos)p=int16_t(sample(s,30000));
    }
    // Guaranteed inside cases plus an ineligible middle/leading/trailing vertex.
    if(k%4==1) { w.joints[3]=w.joints[0];w.joints[3].m[11]=0;w.rigid_records[k%n].joint=3; }
    if(k%4==2) w.blend_records[k%n].weight[0]=12345;
    if(k%4==3) w.blend_records[k%n].count=3;
    fill(w.a,sizeof(w.a));fill(w.b,sizeof(w.b));
    rigid::Job a{w.rigid_records,w.joints,w.a+1,n,320,-224,8,(1u<<29)/8-1};
    auto b=a;b.out=w.b+1;auto& r=l.rows[Rigid];
    auto ra=timed(r.asm_ticks,[&]{return rigid::run(a);});
    auto rb=timed(r.reference_ticks,[&]{return rigid_oracle(b);});
    record(r,k,projected_equal(w,n,ra,rb));
    fill(w.a,sizeof(w.a));fill(w.b,sizeof(w.b));
    blend::Job c{w.blend_records,w.joints,w.a+1,n,320,-224,8,(1u<<29)/8-1};
    auto d=c;d.out=w.b+1;auto& z=l.rows[Blend];
    ra=timed(z.asm_ticks,[&]{return blend::run(c);});
    rb=timed(z.reference_ticks,[&]{return blend_oracle(d);});
    record(z,k,projected_equal(w,n,ra,rb));
}
static void joints(Workspace& w,Lane& l,uint32_t& s,unsigned k) {
    constexpr unsigned count=3;
    fill(w.joints,sizeof(w.joints),0);fill(w.ca,sizeof(w.ca));fill(w.cb,sizeof(w.cb));
    int32_t scaled[9],rows[12];
    for(auto& x:scaled)x=sample(s,70000);
    for(auto& x:rows)x=sample(s,70000);
    for(auto& m:w.joints)for(auto& x:m.m)x=sample(s,70000);
    sat_skin_placement_t p{2,{123,-456,789},100000};
    const unsigned fail=k%count;
    if(k%4==0)w.joints[fail].m[0]=1<<29;
    clip::Job a{w.joints,w.ca+1,count,scaled,rows,&p};auto b=a;b.out=w.cb+1;
    auto& r=l.rows[Clip];
    auto ra=timed(r.asm_ticks,[&]{return clip::run(a);});
    auto rb=timed(r.reference_ticks,[&]{return clip::reference(b);});
    unsigned prefix=k%4==0?fail:count;
    record(r,k,ra==rb && ra==(k%4!=0) && same(w.ca+1,w.cb+1,prefix*sizeof(w.ca[0])) &&
        guard(w.ca,sizeof(w.ca[0])) && guard(w.ca+1+prefix,(4-prefix)*sizeof(w.ca[0])));
}
static void painter(Workspace& w,Lane& l,uint32_t& s,unsigned k,unsigned n) {
    fill(w.ka,sizeof(w.ka));fill(w.kb,sizeof(w.kb));
    unsigned count=k%8==0?0:n;
    for(unsigned i=0;i<count;++i)w.ka[i+1]=w.kb[i+1]=(i%5==0||k%8==1)?paint::kSkip:(rng(s)>>16)%64;
    paint::Range a{},b{};auto& r=l.rows[Range];
    auto ra=timed(r.asm_ticks,[&]{return paint::range(w.ka+1,count,&a);});
    auto rb=timed(r.reference_ticks,[&]{return paint::range_reference(w.kb+1,count,&b);});
    record(r,k,ra==rb&&a.lo==b.lo&&a.hi==b.hi&&same(w.ka,w.kb,sizeof(w.ka)));
    fill(w.ha,sizeof(w.ha));fill(w.hb,sizeof(w.hb));
    for(unsigned i=1;i<=64;++i)w.ha[i]=w.hb[i]=0;
    uint32_t mult=k%2?0x80000000u:0u;
    paint::Spread c{w.ka+1,count,w.ha+1,0,mult},d{w.kb+1,count,w.hb+1,0,mult};
    auto& z=l.rows[Spread];
    timed(z.asm_ticks,[&]{paint::spread(c);return 0u;});
    timed(z.reference_ticks,[&]{paint::spread_reference(d);return 0u;});
    record(z,k,same(w.ka,w.kb,sizeof(w.ka))&&same(w.ha,w.hb,sizeof(w.ha)));
    for(unsigned i=0;i<count;++i)w.ka[i+1]=w.kb[i+1]=(i%5==0)?paint::kSkip:((i%3)<<24)|(rng(s)&0xffffffu);
    paint::Quantise e{w.ka+1,count,0xffffffu,0x55555556u,1000000u},f{w.kb+1,count,0xffffffu,0x55555556u,1000000u};
    auto& q=l.rows[Quantise];
    timed(q.asm_ticks,[&]{paint::quantise(e);return 0u;});
    timed(q.reference_ticks,[&]{paint::quantise_reference(f);return 0u;});
    record(q,k,same(w.ka,w.kb,sizeof(w.ka)));
}
static void geometry(Workspace& w,Lane& l,uint32_t& s,unsigned k,unsigned n) {
    for(unsigned i=0;i<N;++i)w.a[i]={int16_t(sample(s,1024)),int16_t(sample(s,1024)),int32_t(rng(s)&0x7fffffffu)};
    bounds::Bounds a{},b{};auto& r=l.rows[Bounds];
    timed(r.asm_ticks,[&]{bounds::run(w.a,n,&a);return 0u;});
    timed(r.reference_ticks,[&]{bounds::reference(w.a,n,&b);return 0u;});
    record(r,k,same(&a,&b,sizeof(a)));
    fill(w.ea,sizeof(w.ea));fill(w.eb,sizeof(w.eb));fill(w.ka,sizeof(w.ka));fill(w.kb,sizeof(w.kb));
    fill(w.programs,sizeof(w.programs),0);
    for(auto& p:w.programs)for(unsigned i=0;i<6;++i)p.command[i]=uint16_t(rng(s)>>16);
    for(unsigned i=0;i<n;++i) {
        for(unsigned c=0;c<4;++c)w.offsets[i*4+c]=uint16_t(((rng(s)>>16)%N)*sizeof(w.a[0]));
        w.two_sided[i]=uint8_t(i%3==0);w.materials[i]=uint16_t(i%2);
    }
    cull::Job c{w.offsets,w.a,w.two_sided,w.materials,n,w.ea+1,w.ka+1,w.programs,0x03000000u,k%2?16u:0u};
    auto d=c;d.entry=w.eb+1;d.key=w.kb+1;auto& z=l.rows[Cull];
    auto ra=timed(z.asm_ticks,[&]{return cull::run(c);});
    auto rb=timed(z.reference_ticks,[&]{return cull::reference(d);});
    record(z,k,ra==rb&&ra<=n&&same(w.ea,w.eb,sizeof(w.ea))&&same(w.ka,w.kb,sizeof(w.ka)));
    // Independently populate valid program entries, including a stop before a non-program face.
    fill(w.ea,sizeof(w.ea),0);fill(w.commands_a,sizeof(w.commands_a));fill(w.commands_b,sizeof(w.commands_b));
    for(unsigned i=0;i<n;++i) {
        auto& e=w.ea[i];e.kind=SAT_SCENE3D_PROJECTED_PROGRAM;e.program=&w.programs[i%2];
        e.reserved=uint16_t(i%2?i+1:0);e.cached_projected=uint8_t(i%3==0);
        for(auto& xy:e.xy)xy=rng(s);w.order[i]=uint16_t(i);w.indirect[i]=uint16_t(n-1-i);
    }
    if(k%4==0)w.ea[k%n].kind=0;
    program::Job e{w.order,k%2?w.indirect:nullptr,w.ea,w.commands_a+1,n,0};
    auto f=e;f.out=w.commands_b+1;auto& p=l.rows[Program];
    ra=timed(p.asm_ticks,[&]{return program::run(e);});
    rb=timed(p.reference_ticks,[&]{return program::reference(f);});
    record(p,k,ra==rb&&ra<=n&&e.cached==f.cached&&same(w.commands_a,w.commands_b,sizeof(w.commands_a)));
}
static void adx(Lane& l,uint32_t& s,unsigned k) {
    alignas(2) uint8_t frame[18];uint32_t scale=(rng(s)>>16)&0x7fffu;
    frame[0]=uint8_t(scale>>8);frame[1]=uint8_t(scale);
    for(unsigned i=2;i<18;++i)frame[i]=uint8_t(rng(s)>>16);
    int16_t co[2]={int16_t(sample(s,8192)),int16_t(sample(s,8192))};
    int16_t ha[2]={int16_t(sample(s,32767)),int16_t(sample(s,32767))},hb[2]={ha[0],ha[1]};
    int16_t a[34],b[34];fill(a,sizeof(a));fill(b,sizeof(b));auto& r=l.rows[Adx];
    timed(r.asm_ticks,[&]{sat_adx_decode_channel_s16be(frame,co,ha,a+1);return 0u;});
    timed(r.reference_ticks,[&]{
        for(unsigned i=0;i<32;++i) {
            int delta=(frame[2+i/2]>>(i%2?0:4))&15;if(delta>=8)delta-=16;
            int64_t pred=int64_t(co[0])*hb[0]+int64_t(co[1])*hb[1];
            int64_t v=int64_t(delta)*scale+(pred>>12);
            if(v<-32768)v=-32768;if(v>32767)v=32767;
            hb[1]=hb[0];hb[0]=int16_t(v);b[i+1]=hb[0];
        }return 0u;});
    record(r,k,same(a,b,sizeof(a))&&same(ha,hb,sizeof(ha)));
}
static void run_lane(Lane& l,Workspace& w) {
    fill(&l,sizeof(l),0);
    uint32_t sp,sr;asm volatile("mov r15,%0":"=r"(sp));asm volatile("stc sr,%0":"=r"(sr));
    l.on_slave=sp<0x06004000u;l.sr=sr;l.ccr=*reinterpret_cast<volatile uint8_t*>(0xfffffe92u);
    uint32_t seed=0x729a31e5u;
    for(unsigned k=0;k<rounds;++k) {
        unsigned n=k%8==0?1:N;
        projections(w,l,seed,k,n);joints(w,l,seed,k);painter(w,l,seed,k,n);
        geometry(w,l,seed,k,n);adx(l,seed,k);
    }
    l.done=1;
}
static void range_run(void* ctx,uint32_t first,uint32_t last) {
    auto* lanes=static_cast<Lane*>(ctx);
    for(unsigned i=first;i<last;++i)run_lane(lanes[i],work[i]);
}
} // namespace

extern "C" { Report g_sh2_kernel_verify; }
extern "C" unsigned sh2_verify_all(void) {
    auto& r=g_sh2_kernel_verify;
    r.magic=0x534b5631u;r.version=1;r.frequency=sat_time_frequency_hz();
    run_lane(r.lanes[0],work[0]);
    sat_parallel_config_t config{SAT_PARALLEL_SLAVE,nullptr,0,0,60000u};
    r.init_status=uint32_t(-sat_parallel_init(&config));
    r.slave_ready=sat_parallel_slave_available();
    if(!r.init_status&&r.slave_ready)
        r.parallel_status=uint32_t(-sat_parallel_for(0,2,1,range_run,r.lanes+1,r.lanes+1,2*sizeof(Lane)));
    unsigned errors=r.init_status||!r.slave_ready||r.parallel_status;
    for(unsigned i=0;i<3;++i) {
        const auto& l=r.lanes[i];if(!l.done||l.on_slave!=(i==2)||(l.sr&2u))++errors;
        for(const auto& k:l.rows)errors+=k.errors+(k.cases!=rounds);
    }
    asm volatile("" ::: "memory");r.done=1;
    return errors;
}
