#include <cassert>
#include <cstdio>
#include "examples/killer_clown_demo/lighting.h"
static sat_vec3_t point(int x,int y,int z) {return {x*SAT_FX16_ONE,y*SAT_FX16_ONE,z*SAT_FX16_ONE};}
static sat_vec3_t reference_normalize(sat_vec3_t n) {
    auto magnitude=[](int32_t v){return v<0?-v:v;};
    while(magnitude(n.x)>CLOWN_NORMAL_LIMIT || magnitude(n.y)>CLOWN_NORMAL_LIMIT ||
          magnitude(n.z)>CLOWN_NORMAL_LIMIT) {n.x>>=1;n.y>>=1;n.z>>=1;}
    auto length=sat_isqrt32((uint32_t)(n.x*n.x+n.y*n.y+n.z*n.z));
    if(!length)return {0,0,0};
    return {n.x*SAT_FX16_ONE/(int32_t)length,n.y*SAT_FX16_ONE/(int32_t)length,
        n.z*SAT_FX16_ONE/(int32_t)length};
}
static void normalization_matches_integer_reference() {
    uint32_t state=0x451ab;
    for(unsigned i=0;i<200000;++i) {
        sat_vec3_t n;
        int32_t* components[]={&n.x,&n.y,&n.z};
        for(auto component:components) {
            state=state*1664525u+1013904223u;
            *component=(int32_t)(state&0x0fffffffu)-0x08000000;
            if(i<4096)*component=(*component%1025);
        }
        if(i==0)n={0,0,0};
        if(i==1)n={1,-1,0};
        if(i==2)n={1024,-1024,1024};
        const auto expected=reference_normalize(n),actual=clown_light_normalize(n);
        assert(actual.x==expected.x && actual.y==expected.y && actual.z==expected.z);
    }
    for(unsigned shift=0;shift<=20;++shift)for(int residual=-2;residual<=2;++residual) {
        const int32_t value=(CLOWN_NORMAL_LIMIT<<shift)+residual;
        const sat_vec3_t n={-value,value/3,-value/7};
        const auto expected=reference_normalize(n),actual=clown_light_normalize(n);
        assert(actual.x==expected.x && actual.y==expected.y && actual.z==expected.z);
    }
    const auto minimum=clown_light_normalize({INT32_MIN,0,0});
    assert(minimum.x==-SAT_FX16_ONE && minimum.y==0 && minimum.z==0);
}
int main() {
    normalization_matches_integer_reference();
    clown_spotlight_t light;
    clown_spotlight_aim(&light,point(0,4,0),point(0,1,0));
    assert(clown_spotlight_strength(&light,point(0,1,0))>0);
    assert(clown_spotlight_strength(&light,point(0,5,0))==0);
    assert(clown_spotlight_strength(&light,point(2,1,0))==0);
    assert(clown_spotlight_strength(&light,point(0,-9,0))==0);
    assert(clown_spotlight_diffuse(&light,point(0,1,0),point(0,1,0))>0);
    assert(clown_spotlight_diffuse(&light,point(0,1,0),point(0,-1,0))==0);
    sat_vec3_t edge={SAT_FX16_ONE/2,3*SAT_FX16_ONE,0};
    assert(clown_spotlight_strength(&light,edge)==0);
    edge.y=0;assert(clown_spotlight_strength(&light,edge)>0);
    for(int angle=0;angle<360;angle+=45) {
        sat_vec3_t rim;assert(clown_spotlight_ground(&light,angle*SAT_FX16_ONE,&rim));
        assert(rim.y==0 && (rim.x || rim.z));
    }
    clown_spotlight_aim(&light,point(1,3,2),point(0,1,0));
    assert(clown_spotlight_diffuse(&light,point(0,1,0),point(0,0,1))>0);
    assert(clown_spotlight_diffuse(&light,point(0,1,0),point(0,0,-1))==0);
    std::puts("clown spotlight: exact normalization reference, XYZ cone, floor intersection and front-surface illumination OK");
}
