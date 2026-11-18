#include <stdint.h>
#include "saturn/time.h"
#include "saturn/parallel.h"
#include "src/graphics/3d/rendering/logic.hpp"

namespace {
namespace render=saturn::core::render3d;
namespace pk=render::paint_keys;
namespace pd=render::paint_detail;
constexpr unsigned N=2048,B=1024,R=32,S=16;
struct Row {
    uint32_t cases,errors,first_error,faces,live,range_ticks,spread_ticks,
             prefix_cpp_ticks,prefix_asm_ticks,place_cpp_ticks,place_asm_ticks,words;
};
struct alignas(16) Lane {
    uint32_t slave,sr,ccr,done,edges,edge_errors,reserved[2];
    Row rows[S];
};
struct alignas(16) Report {
    uint32_t magic,version,done,init_status,slave_ready,parallel_status,frequency,reserved;
    Lane lanes[3];
};
static_assert(sizeof(Row)==48&&sizeof(Lane)==800&&sizeof(Report)==2432);
union Counters {
    uint32_t word[B/2+2];
    uint16_t half[B+4];
};
struct Work {
    uint32_t original[N],keys[N];
    Counters a,b,expected;
    unsigned starts[B];
    uint16_t order_a[N+2],order_b[N+2],oracle[N+2];
    uint8_t byte_a[257],byte_b[257],byte_oracle[257];
};
static Work work[2];
uint32_t rng(uint32_t& s){s=s*1664525u+1013904223u;return s;}
template<class T> bool same(const T* a,const T* b,unsigned n){
    for(unsigned i=0;i<n;++i)if(a[i]!=b[i])return false;
    return true;
}
void fill(Counters& c,unsigned words){
    for(unsigned i=0;i<B/2+2;++i)c.word[i]=0xa55aa55au;
    for(unsigned i=0;i<words;++i)c.word[i+1]=0;
}
void edges(Lane& l){
    // Prefix boundary: sum of live counters may be 65535, not signed 32767.
    alignas(4) uint16_t c[4]={},expected[4]={};
    c[0]=65534;c[1]=1;
    expected[0]=1;expected[1]=0;
    pk::sat_paint_prefix_sh2(reinterpret_cast<uint32_t*>(c),1);
    ++l.edges;if(!same(c,expected,4))++l.edge_errors;
    for(unsigned i=0;i<4;++i)c[i]=expected[i]=0;
    c[0]=32768;c[2]=32767;
    expected[0]=32767;expected[1]=32767;
    pk::sat_paint_prefix_sh2(reinterpret_cast<uint32_t*>(c),2);
    ++l.edges;if(!same(c,expected,4))++l.edge_errors;
    // Empty native calls must not read/write any buffers.
    pk::sat_paint_prefix_sh2(nullptr,0);
    pk::sat_paint_place8_sh2(nullptr,0,nullptr,nullptr);
    pk::sat_paint_place16_sh2(nullptr,0,nullptr,nullptr);
    l.edges+=3;
}
template<class I>
void place(Work& w,Row& r,unsigned n,unsigned live,unsigned words,unsigned k,I* a,I* b,I* expected,unsigned cap){
    for(unsigned i=0;i<cap;++i)a[i]=b[i]=expected[i]=I(0xa5a5u);
    auto original=[&]{auto t=sat_time_frc();pd::place_reference(w.keys,n,a+1,w.a.half+2);
        r.place_cpp_ticks+=uint16_t(sat_time_frc()-t);};
    auto candidate=[&]{auto t=sat_time_frc();
        if constexpr(sizeof(I)==1)pk::sat_paint_place8_sh2(w.keys,n,b+1,w.b.half+2);
        else pk::sat_paint_place16_sh2(w.keys,n,b+1,w.b.half+2);
        r.place_asm_ticks+=uint16_t(sat_time_frc()-t);};
    if(k&1){candidate();original();}else{original();candidate();}
    // Independent oracle: starts computed one bucket at a time, and keys
    // independently mapped from original input before this function.
    auto* starts=w.starts;
    unsigned running=0;
    for(unsigned j=words*2;j>0;){--j;starts[j]=running;running+=w.expected.half[j+2];}
    bool ok=running==live;
    for(unsigned i=0;i<n;++i)if(w.keys[i]!=pk::kSkip)
        expected[1+starts[w.keys[i]]++]=I(i);
    for(unsigned j=0;j<words*2;++j)w.expected.half[j+2]=uint16_t(starts[j]);
    ok=ok&&same(a,expected,cap)&&same(b,expected,cap)&&
        same(w.a.word,w.expected.word,B/2+2)&&same(w.b.word,w.expected.word,B/2+2);
    if(!ok){if(!r.errors)r.first_error=k;++r.errors;}
}
void scenario(Work& w,Row& r,unsigned sc,unsigned k){
    constexpr unsigned sizes[S]={0,1,255,256,96,512,1024,1800,2048,2048,2048,2048,1024,255,2048,2048};
    const unsigned n=sizes[sc];
    uint32_t seed=0x386ab127u+k*0x9e3779b9u+sc;
    uint32_t lo=UINT32_MAX,hi=0,live=0;
    for(unsigned i=0;i<n;++i){
        uint32_t x=rng(seed),key=x;
        switch(sc){
            case 1:key=9;break;
            case 2:key=i%17;break;
            case 3:key=255-i;break;
            case 4:key=pk::kSkip;break;
            case 5:key=12345;break;
            case 6:key=i;break;
            case 7:key=(x>>12)&65535;break;
            case 8:key=(i%8==0)?pk::kSkip:x;break;
            case 9:key=(i%8!=0)?pk::kSkip:x;break;
            case 10:key=(i%2)?0xfffffffeu:0;break;
            case 11:key=(i%3==0)?1023:1022;break;
            case 12:key=((i%4)<<28)|(x&0x000fffffu);break;
            case 13:key=(i%2)?pk::kSkip:(x&0x1ff);break;
            case 14:key=(i==0||i+1==n)?0x7fffffffu:pk::kSkip;break;
            case 15:key=(i%3==0)?255:((i%3==1)?256:1023);break;
            default:break;
        }
        w.original[i]=w.keys[i]=key;
        if(key!=pk::kSkip){++live;if(key<lo)lo=key;if(key>hi)hi=key;}
    }
    auto t=sat_time_frc();pk::Range range;
    const uint32_t actual=pk::range(w.keys,n,&range);
    r.range_ticks+=uint16_t(sat_time_frc()-t);
    bool ok=actual==live&&range.lo==lo&&range.hi==hi;
    unsigned shift=0,words=0;
    if(live){while(((hi-lo)>>shift)>=B)++shift;words=((hi-lo)>>shift)/2+1;}
    fill(w.a,words);fill(w.b,words);fill(w.expected,words);
    const uint32_t mult=shift?uint32_t(1u<<(32-shift)):0;
    t=sat_time_frc();pk::spread(pk::Spread{w.keys,n,w.a.half+2,lo,mult});
    r.spread_ticks+=uint16_t(sat_time_frc()-t);
    for(unsigned i=0;i<n;++i){
        const uint32_t mapped=w.original[i]==pk::kSkip?pk::kSkip:(w.original[i]-lo)>>shift;
        ok=ok&&w.keys[i]==mapped;
        if(mapped!=pk::kSkip)++w.expected.half[mapped+2];
    }
    ok=ok&&same(w.a.word,w.expected.word,B/2+2);
    // Stop before unsafe indexing if a prior stage is wrong.
    if(!ok){if(!r.errors)r.first_error=k;++r.errors;++r.cases;r.faces+=n;r.live+=live;r.words+=words;return;}
    w.b=w.a;
    auto cpp=[&]{auto begin=sat_time_frc();pd::prefix_reference(w.a.word+1,words);
        r.prefix_cpp_ticks+=uint16_t(sat_time_frc()-begin);};
    auto native=[&]{auto begin=sat_time_frc();pk::sat_paint_prefix_sh2(w.b.word+1,words);
        r.prefix_asm_ticks+=uint16_t(sat_time_frc()-begin);};
    if(k&1){native();cpp();}else{cpp();native();}
    uint32_t running=0;
    for(unsigned i=words*2;i>0;){--i;
        ok=ok&&w.a.half[i+2]==running&&w.b.half[i+2]==running;
        running+=w.expected.half[i+2];}
    if(!ok||running!=live){if(!r.errors)r.first_error=k;++r.errors;}
    else if(n<=255)place(w,r,n,live,words,k,w.byte_a,w.byte_b,w.byte_oracle,257);
    else place(w,r,n,live,words,k,w.order_a,w.order_b,w.oracle,N+2);
    ++r.cases;r.faces+=n;r.live+=live;r.words+=words;
}
struct Context{Lane* lanes;unsigned round,sc;};
void run(void* ptr,uint32_t first,uint32_t last){
    const auto& c=*static_cast<Context*>(ptr);
    for(unsigned i=first;i<last;++i){
        auto& l=c.lanes[i];
        if(!c.round&&!c.sc){l={};uint32_t sp,sr;
            asm volatile("mov r15,%0":"=r"(sp));asm volatile("stc sr,%0":"=r"(sr));
            l.slave=sp<0x06004000u;l.sr=sr;l.ccr=*reinterpret_cast<volatile uint8_t*>(0xfffffe92u);edges(l);}
        scenario(work[i],l.rows[c.sc],c.sc,c.round);
        if(c.round+1==R&&c.sc+1==S)l.done=1;
    }
}
}
extern "C" {Report g_paint_distribution_bench;}
extern "C" unsigned paint_distribution_verify(void){
    auto& r=g_paint_distribution_bench;r.magic=0x50444231u;r.version=1;r.frequency=sat_time_frequency_hz();
    for(unsigned k=0;k<R;++k)for(unsigned sc=0;sc<S;++sc){Context c{r.lanes,k,sc};run(&c,0,1);}
    sat_parallel_config_t cfg{SAT_PARALLEL_SLAVE,nullptr,0,0,60000u};
    r.init_status=uint32_t(-sat_parallel_init(&cfg));r.slave_ready=sat_parallel_slave_available();
    if(!r.init_status&&r.slave_ready)for(unsigned k=0;k<R&&!r.parallel_status;++k)
        for(unsigned sc=0;sc<S&&!r.parallel_status;++sc){Context c{r.lanes+1,k,sc};
            r.parallel_status=uint32_t(-sat_parallel_for(0,2,1,run,&c,r.lanes+1,2*sizeof(Lane)));}
    unsigned errors=r.init_status||!r.slave_ready||r.parallel_status;
    for(unsigned i=0;i<3;++i){auto& l=r.lanes[i];errors+=l.edge_errors+(!l.done)+(l.slave!=(i==2));
        for(auto& row:l.rows)errors+=row.errors+(row.cases!=R);}
    asm volatile("":::"memory");r.done=1;return errors;
}
