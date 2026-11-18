#include <stdint.h>
#include "saturn/time.h"
#include "saturn/parallel.h"
#include "src/graphics/3d/rendering/logic.hpp"
namespace {
namespace r=saturn::core::render3d;
constexpr unsigned N=2048,B=1024,S=24,R=32;
struct Row{uint32_t cases,errors,first_error,faces,live,control_ticks,byte_ticks,v2_ticks;};
struct alignas(16) Lane{uint32_t slave,sr,ccr,done,edges,edge_errors,reserved[2];Row rows[S];};
struct alignas(16) Report{uint32_t magic,version,done,init_status,slave_ready,parallel_status,frequency,reserved;Lane lanes[3];};
static_assert(sizeof(Lane)==800&&sizeof(Report)==2432);
struct Work{
    uint32_t keys[3][N+2],original[N],expected[N];
    uint16_t wide[3][N+2],wide_ref[N+2];
    uint8_t narrow[3][257],narrow_ref[257];
    uint32_t hist[B],starts[B];
};
static Work work[2];
uint32_t rng(uint32_t& s){s=s*1664525u+1013904223u;return s;}
template<class T>bool same(const T* a,const T* b,unsigned n){for(unsigned i=0;i<n;++i)if(a[i]!=b[i])return false;return true;}
struct Group{uint32_t id,lo,hi,n,base,shift;};
uint32_t shr(uint32_t span,uint32_t cap){uint32_t s=0;while((span>>s)>=cap)++s;return s;}
// Independent mapping oracle: groups are sorted during insertion rather than
// last-group caching. Direct variable shifts, not reciprocal multipliers.
uint32_t reference(Work& w,unsigned n,unsigned gs){
    uint32_t lo=UINT32_MAX,hi=0,live=0,ng=0;
    Group groups[8]={};bool fallback=false;
    for(unsigned i=0;i<n;++i){
        uint32_t key=w.original[i];if(key==UINT32_MAX)continue;
        ++live;if(key<lo)lo=key;if(key>hi)hi=key;
        if(!gs||fallback)continue;
        uint32_t id=key>>gs,depth=key&((1u<<gs)-1);
        unsigned j=0;while(j<ng&&groups[j].id<id)++j;
        if(j==ng||groups[j].id!=id){
            if(ng==8){fallback=true;continue;}
            for(unsigned k=ng;k>j;--k)groups[k]=groups[k-1];
            groups[j]={id,depth,depth,0,0,0};++ng;
        }
        auto& g=groups[j];++g.n;if(depth<g.lo)g.lo=depth;if(depth>g.hi)g.hi=depth;
    }
    if(!live){for(unsigned i=0;i<n;++i)w.expected[i]=UINT32_MAX;return 0;}
    const bool grouped=gs&&!fallback&&ng>1;
    uint32_t base=0;
    if(grouped)for(unsigned j=0;j<ng;++j){
        auto& g=groups[j];const uint32_t share=1+((B-ng)*g.n)/live;
        g.base=base;g.shift=shr(g.hi-g.lo,share);base+=share;
    }
    const uint32_t global_shift=shr(hi-lo,B);
    for(unsigned i=0;i<n;++i){
        const uint32_t key=w.original[i];
        if(key==UINT32_MAX){w.expected[i]=key;continue;}
        if(!grouped)w.expected[i]=(key-lo)>>global_shift;
        else{
            const uint32_t id=key>>gs;
            unsigned j=0;while(groups[j].id!=id)++j;
            const auto& g=groups[j];
            w.expected[i]=g.base+(((key&((1u<<gs)-1))-g.lo)>>g.shift);
        }
    }
    return live;
}
template<class I,unsigned M>uint32_t sort(uint32_t* k,unsigned n,I* out,unsigned gs){
    return gs?r::paint_order_grouped_buckets<I,M>(k,n,out,gs):r::paint_order_buckets<I,M>(k,n,out);
}
template<class I>
bool run_sort(Work& w,Row& row,unsigned n,unsigned gs,unsigned k,I (&order)[3][(sizeof(I)==1?257:N+2)],I* ref){
    constexpr unsigned cap=sizeof(I)==1?257:N+2;
    for(unsigned m=0;m<3;++m){
        w.keys[m][0]=0x12345678;w.keys[m][N+1]=0x87654321;
        for(unsigned i=0;i<N;++i)w.keys[m][i+1]=i<n?w.original[i]:0xabcdef12;
        for(unsigned i=0;i<cap;++i)order[m][i]=I(0xa5a5);
    }
    for(unsigned i=0;i<cap;++i)ref[i]=I(0xa5a5);
    const uint32_t live=reference(w,n,gs);
    for(unsigned i=0;i<B;++i)w.hist[i]=0;
    for(unsigned i=0;i<n;++i)if(w.expected[i]!=UINT32_MAX)++w.hist[w.expected[i]];
    unsigned running=0;
    for(unsigned i=B;i>0;){--i;w.starts[i]=running;running+=w.hist[i];}
    for(unsigned i=0;i<n;++i)if(w.expected[i]!=UINT32_MAX)ref[1+w.starts[w.expected[i]]++]=I(i);
    uint32_t got[3]={};
    auto a=[&]{auto t=sat_time_frc();got[0]=sort<I,0>(w.keys[0]+1,n,order[0]+1,gs);row.control_ticks+=uint16_t(sat_time_frc()-t);};
    auto b=[&]{auto t=sat_time_frc();got[1]=sort<I,1>(w.keys[1]+1,n,order[1]+1,gs);row.byte_ticks+=uint16_t(sat_time_frc()-t);};
    auto c=[&]{auto t=sat_time_frc();got[2]=sort<I,2>(w.keys[2]+1,n,order[2]+1,gs);row.v2_ticks+=uint16_t(sat_time_frc()-t);};
    switch(k%6){case 0:a();b();c();break;case 1:c();b();a();break;case 2:b();c();a();break;case 3:a();c();b();break;case 4:c();a();b();break;default:b();a();c();}
    row.live+=live;
    bool ok=running==live;
    for(unsigned m=0;m<3;++m){
        ok=ok&&got[m]==live&&same(order[m],ref,cap)&&same(w.keys[m]+1,w.expected,n);
        ok=ok&&w.keys[m][0]==0x12345678&&w.keys[m][N+1]==0x87654321;
        for(unsigned i=n;i<N;++i)ok=ok&&w.keys[m][i+1]==0xabcdef12;
    }
    return ok;
}
void scenario(Work& w,Row& row,unsigned sc,unsigned k){
    constexpr unsigned counts[S]={0,1,255,256,96,512,1024,1800,2048,2048,2048,2048,255,2048,2048,2048,2048,2048,2048,255,2048,2048,2048,255};
    const unsigned n=counts[sc];
    unsigned gs=0;
    if(sc>=14)gs=sc==21?1:sc==22?31:24;
    uint32_t seed=0x386ab127u+k*0x9e3779b9u+sc;
    for(unsigned i=0;i<n;++i){
        uint32_t x=rng(seed),key=x;
        switch(sc){
            case 1:key=9;break;
            case 2:key=i%17;break;
            case 3:key=255-i;break;
            case 4:key=UINT32_MAX;break;
            case 5:key=12345;break;
            case 6:key=i;break;
            case 7:key=(x>>12)&65535;break;
            case 8:key=i%8?x:UINT32_MAX;break;
            case 9:key=i%8?UINT32_MAX:x;break;
            case 10:key=i%2?0xfffffffeu:0;break;
            case 11:key=i%3==0?1023:1022;break;
            case 12:key=i%2?UINT32_MAX:(x&511);break;
            case 13:key=(i==0||i+1==n)?0x7fffffffu:UINT32_MAX;break;
            case 14:key=(3u<<24)|(x&65535);break;
            case 15:key=((i/1024)<<24)|(x&65535);break;
            case 16:key=((i%4)<<24)|(x&0x00ffffff);break;
            case 17:key=((i%8)<<24)|(x&0xffff);break;
            case 18:key=((i%9)<<24)|(x&0xffff);break;
            case 19:key=((i%4)<<24)|(x&0xffff);break;
            case 20:key=i%5?((i%4)<<24)|(x&65535):UINT32_MAX;break;
            case 21:key=i%18;break;
            case 22:key=(i%2?0x80000000u:0)|(x&0x7fffffffu);break;
            case 23:key=i%2?UINT32_MAX:0x03001234u;break;
            default:break;
        }
        w.original[i]=key;
    }
    bool ok=n<=255?run_sort<uint8_t>(w,row,n,gs,k,w.narrow,w.narrow_ref):run_sort<uint16_t>(w,row,n,gs,k,w.wide,w.wide_ref);
    ++row.cases;row.faces+=n;if(!ok){if(!row.errors)row.first_error=k;++row.errors;}
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
extern "C" {Report g_paint_order_bench;}
extern "C" unsigned paint_order_verify(void){
    auto& r=g_paint_order_bench;r.magic=0x504f4232u;r.version=2;r.frequency=sat_time_frequency_hz();
    for(unsigned k=0;k<R;++k)for(unsigned sc=0;sc<S;++sc){Context c{r.lanes,k,sc};run(&c,0,1);}
    sat_parallel_config_t cfg{SAT_PARALLEL_SLAVE,nullptr,0,0,60000u};
    r.init_status=uint32_t(-sat_parallel_init(&cfg));r.slave_ready=sat_parallel_slave_available();
    if(!r.init_status&&r.slave_ready)for(unsigned k=0;k<R&&!r.parallel_status;++k)
        for(unsigned sc=0;sc<S&&!r.parallel_status;++sc){Context c{r.lanes+1,k,sc};r.parallel_status=uint32_t(-sat_parallel_for(0,2,1,run,&c,r.lanes+1,2*sizeof(Lane)));}
    unsigned errors=r.init_status||!r.slave_ready||r.parallel_status;
    for(unsigned i=0;i<3;++i){auto& l=r.lanes[i];errors+=(!l.done)+(l.slave!=(i==2));for(auto& row:l.rows)errors+=row.errors+(row.cases!=R);}
    asm volatile("":::"memory");r.done=1;return errors;
}
