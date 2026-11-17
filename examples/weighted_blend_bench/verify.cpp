#include <stdint.h>
#include "saturn/skin3d.h"
#include "saturn/time.h"
#include "saturn/parallel.h"
#include "src/graphics/3d/animation/weighted_delta_sh2.hpp"
extern "C" sat_result_t sat_skin_deform_range_weighted_delta(
    const sat_skin_asset_t*,const sat_mat4_t*,uint16_t,uint32_t,uint32_t,sat_vec3_t*,uint32_t);
extern "C" sat_result_t sat_skin_deform_range_weighted_selective(
    const sat_skin_asset_t*,const sat_mat4_t*,uint16_t,uint32_t,uint32_t,sat_vec3_t*,uint32_t);
namespace {
constexpr unsigned N=96,J=16,R=64,S=12;
struct Row {uint32_t cases,errors,first_error,control_ticks,candidate_ticks,vertices,pair_axes,triple_axes,fallback_axes,selective_ticks;};
struct alignas(16) Lane {uint32_t slave,sr,ccr,done,edge_checks,edge_errors,reserved[2];Row rows[S];};
struct alignas(16) Report {uint32_t magic,version,done,init_status,slave_ready,parallel_status,frequency,reserved;Lane lanes[3];};
static_assert(sizeof(Lane)==512&&sizeof(Report)==1568);
struct Work {sat_skin_rigid_t rigid[N];sat_skin_blend_t blend[N];sat_mat4_t mats[J];sat_vec3_t a[N+2],b[N+2],c[N+2],ref[N+2];};
static Work work[2];
uint32_t rng(uint32_t& s){s=s*1664525u+1013904223u;return s;}
int32_t random(uint32_t& s,int32_t r){return int32_t((rng(s)>>8)%uint32_t(2*r+1))-r;}
int32_t floorq(int64_t x,int32_t d){return int32_t(x/d-(x<0&&x%d!=0));}
bool equal(const sat_vec3_t& a,const sat_vec3_t& b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
int32_t axis(const sat_mat4_t& m,const int16_t* q,unsigned r,int32_t scale){
    int64_t sum=32768;
    for(unsigned k=0;k<3;++k)sum+=int64_t(m.m[r*4+k])*q[k]*scale;
    return floorq(sum,65536)+m.m[r*4+3];
}
bool fits(int32_t a,int32_t b){const int64_t d=int64_t(a)-b;return d>=INT32_MIN&&d<=INT32_MAX;}
void oracle(Work& w,const sat_skin_asset_t& s,unsigned first,unsigned n,Row& row){
    for(unsigned i=first;i<first+n;++i){
        int32_t result[3];
        for(unsigned r=0;r<3;++r){
            if(i<s.rigid_count){
                const auto& q=s.rigid[i];result[r]=axis(w.mats[q.joint],q.pos,r,1<<(16-s.pos_shift));
                continue;
            }
            const auto& q=s.blend[i-s.rigid_count];
            int32_t p[3]={};
            for(unsigned k=0;k<q.count;++k)p[k]=axis(w.mats[q.joint[k]],q.pos[k],r,1<<(16-s.pos_shift));
            if(q.count==1)result[r]=p[0];
            else if(q.count==2&&q.weight[0]==16384)result[r]=floorq(int64_t(p[0])+p[1],2);
            else if(q.count==2){
                ++row.pair_axes;if(!fits(p[0],p[1]))++row.fallback_axes;
                result[r]=floorq(int64_t(q.weight[0])*p[0]+int64_t(32768-q.weight[0])*p[1]+16384,32768);
            }else{
                ++row.triple_axes;if(!fits(p[0],p[2])||!fits(p[1],p[2]))++row.fallback_axes;
                result[r]=floorq(int64_t(q.weight[0])*p[0]+int64_t(q.weight[1])*p[1]+
                    int64_t(32768-q.weight[0]-q.weight[1])*p[2]+16384,32768);
            }
        }
        w.ref[i+1]={result[0],result[1],result[2]};
    }
}
void edge(Lane& l,unsigned chunk){
    using namespace saturn::skin3d::weighted_delta;
    const int32_t p[]={INT32_MIN,INT32_MIN+1,-65537,-1,0,1,65536,INT32_MAX};
    const int32_t weights[]={0,1,16383,16384,16385,32767,32768};
    const auto a=p[chunk];
    for(auto b:p)for(auto w:weights){
        ++l.edge_checks;
        if(pair(a,b,w)!=floorq(int64_t(a)*w+int64_t(b)*(32768-w)+16384,32768))++l.edge_errors;
        ++l.edge_checks;
        if(pair_selective(a,b,w)!=floorq(int64_t(a)*w+int64_t(b)*(32768-w)+16384,32768))++l.edge_errors;
        for(auto c:p){
            int32_t v=(32768-w)/3;
            ++l.edge_checks;
            if(triple(a,b,c,w,v)!=floorq(int64_t(a)*w+int64_t(b)*v+int64_t(c)*(32768-w-v)+16384,32768))++l.edge_errors;
        }
    }
}
void scenario(Work& w,Row& r,unsigned sc,unsigned k){
    uint32_t seed=0x8bb03131u+k*0x9e3779b9u+sc;
    unsigned total=sc==0?0:sc==1?1:N;
    unsigned rigid=sc==2?total:sc==7||sc==10?N/3:0;
    unsigned shift=sc==8?12:sc==9?0:15;
    unsigned first=sc==10?N/3-3:0;
    unsigned count=sc==10?47:total;
    for(unsigned i=0;i<J;++i){
        auto& m=w.mats[i];m={};
        for(unsigned a=0;a<3;++a){
            for(unsigned b=0;b<3;++b)m.m[a*4+b]=random(seed,18000)+(a==b?65536:0);
            m.m[a*4+3]=random(seed,65536);
            if(sc==11){
                for(unsigned b=0;b<3;++b)m.m[a*4+b]=0;
                m.m[a*4+3]=(i&1)?INT32_MAX-int32_t(i):INT32_MIN+int32_t(i);
            }
        }
        m.m[15]=65536;
    }
    for(unsigned i=0;i<rigid;++i){
        auto& q=w.rigid[i];q={};q.joint=uint8_t(i%J);
        for(auto& p:q.pos)p=int16_t(random(seed,shift==0?32:32767));
    }
    uint8_t highest=0;
    for(unsigned i=0;i<rigid;++i)if(w.rigid[i].joint>highest)highest=w.rigid[i].joint;
    for(unsigned i=0;i<total-rigid;++i){
        auto& q=w.blend[i];q={};
        q.count=sc==3?1:sc==4||sc==5||sc==1?2:sc==7||sc==10||sc==11?uint8_t(1+i%3):3;
        q.weight[0]=sc==4?16384:uint16_t(rng(seed)%32769);
        if(sc==5&&q.weight[0]==16384)q.weight[0]=16385;
        q.weight[1]=uint16_t(rng(seed)%(32769-q.weight[0]));
        if(i%13==0&&sc!=4){q.weight[0]=0;q.weight[1]=32768;}
        if(i%17==0&&sc!=4){q.weight[0]=32768;q.weight[1]=0;}
        for(unsigned j=0;j<q.count;++j){
            q.joint[j]=uint8_t((i+j*7)%J);
            if(q.joint[j]>highest)highest=q.joint[j];
            for(auto& p:q.pos[j])p=int16_t(random(seed,shift==0?32:32767));
        }
    }
    sat_skin_asset_t skin{w.rigid,w.blend,nullptr,uint16_t(rigid),uint16_t(total-rigid),J,uint8_t(shift),highest};
    bool ok=sat_skin_validate(&skin)==SAT_OK;
    for(unsigned i=0;i<N+2;++i)w.a[i]=w.b[i]=w.c[i]=w.ref[i]={0x12345678,-12345678,0x11223344};
    sat_result_t a=SAT_ERR_INVALID_ARG,b=a,c=a;
    auto control=[&]{auto t=sat_time_frc();a=sat_skin_deform_range(&skin,w.mats,J,first,count,w.a+1,N);
        r.control_ticks+=uint16_t(sat_time_frc()-t);};
    auto candidate=[&]{auto t=sat_time_frc();b=sat_skin_deform_range_weighted_delta(&skin,w.mats,J,first,count,w.b+1,N);
        r.candidate_ticks+=uint16_t(sat_time_frc()-t);};
    auto selective=[&]{auto t=sat_time_frc();c=sat_skin_deform_range_weighted_selective(&skin,w.mats,J,first,count,w.c+1,N);
        r.selective_ticks+=uint16_t(sat_time_frc()-t);};
    switch(k%6){
        case 0:control();candidate();selective();break;
        case 1:selective();candidate();control();break;
        case 2:candidate();selective();control();break;
        case 3:control();selective();candidate();break;
        case 4:selective();control();candidate();break;
        default:candidate();control();selective();break;
    }
    oracle(w,skin,first,count,r);
    ok=ok&&a==SAT_OK&&b==a&&c==a;
    for(unsigned i=0;i<N+2;++i)ok=ok&&equal(w.a[i],w.ref[i])&&equal(w.b[i],w.ref[i])&&equal(w.c[i],w.ref[i]);
    // Rejected range must leave output untouched.
    auto bad=sat_skin_deform_range_weighted_delta(&skin,w.mats,J,total+1,1,w.b+1,N);
    ok=ok&&bad==SAT_ERR_INVALID_ARG;
    bad=sat_skin_deform_range_weighted_selective(&skin,w.mats,J,total+1,1,w.c+1,N);
    ok=ok&&bad==SAT_ERR_INVALID_ARG;
    for(unsigned i=0;i<N+2;++i)ok=ok&&equal(w.b[i],w.ref[i]);
    ++r.cases;r.vertices+=count;
    if(!ok){if(!r.errors)r.first_error=k;++r.errors;}
}
struct Context{Lane* lanes;unsigned round,sc;};
void run(void* p,uint32_t first,uint32_t last){
    const auto& c=*static_cast<Context*>(p);
    for(unsigned i=first;i<last;++i){auto& l=c.lanes[i];
        if(!c.round&&!c.sc){l={};uint32_t sp,sr;asm volatile("mov r15,%0":"=r"(sp));asm volatile("stc sr,%0":"=r"(sr));
            l.slave=sp<0x06004000u;l.sr=sr;l.ccr=*reinterpret_cast<volatile uint8_t*>(0xfffffe92u);}
        if(c.sc==0&&c.round<8)edge(l,c.round);
        scenario(work[i],l.rows[c.sc],c.sc,c.round);
        if(c.round+1==R&&c.sc+1==S)l.done=1;
    }
}
}
extern "C" {Report g_weighted_blend_bench;}
extern "C" unsigned weighted_blend_verify(void){
    auto& r=g_weighted_blend_bench;r.magic=0x57424231u;r.version=2;r.frequency=sat_time_frequency_hz();
    for(unsigned k=0;k<R;++k)for(unsigned sc=0;sc<S;++sc){Context c{r.lanes,k,sc};run(&c,0,1);}
    sat_parallel_config_t config{SAT_PARALLEL_SLAVE,nullptr,0,0,60000u};
    r.init_status=uint32_t(-sat_parallel_init(&config));r.slave_ready=sat_parallel_slave_available();
    if(!r.init_status&&r.slave_ready)for(unsigned k=0;k<R&&!r.parallel_status;++k)
        for(unsigned sc=0;sc<S&&!r.parallel_status;++sc){Context c{r.lanes+1,k,sc};
            r.parallel_status=uint32_t(-sat_parallel_for(0,2,1,run,&c,r.lanes+1,2*sizeof(Lane)));}
    unsigned errors=r.init_status||!r.slave_ready||r.parallel_status;
    for(unsigned i=0;i<3;++i){auto& l=r.lanes[i];errors+=l.edge_errors+(!l.done)+(l.slave!=(i==2))+(!!(l.sr&2u));
        for(auto& row:l.rows)errors+=row.errors+(row.cases!=R);}
    asm volatile("":::"memory");r.done=1;return errors;
}
