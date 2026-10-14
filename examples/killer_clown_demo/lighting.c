#include "lighting.h"
/* Each normalized component is bounded to +/-1024 before division. A
 * reciprocal estimate followed by one remainder correction gives exactly
 * the C integer quotient, without three software divides per vector.
 * Master primes the table while aiming the lights, before any shading range.
 * Workers may read the immutable table only after that initialization and
 * cache publication; concurrent first-use initialization is unsupported. */
enum { NORMAL_RECIP_CAPACITY=CLOWN_NORMAL_LIMIT*2 };
static uint32_t normal_reciprocals[NORMAL_RECIP_CAPACITY] __attribute__((section(".wram_l")));
static unsigned normal_reciprocals_ready;
static int32_t normal_component(int32_t component,uint32_t length) {
    int32_t numerator=component*SAT_FX16_ONE;
    uint32_t absolute=(uint32_t)(numerator<0?-numerator:numerator);
    uint32_t quotient=(uint32_t)(((uint64_t)absolute*normal_reciprocals[length])>>32);
    if(absolute-quotient*length>=length)++quotient;
    return numerator<0?-(int32_t)quotient:(int32_t)quotient;
}
static uint32_t magnitude(sat_fx16_t value) {return value<0?0u-(uint32_t)value:(uint32_t)value;}
sat_vec3_t clown_light_normalize(sat_vec3_t n) {
    uint32_t maximum=magnitude(n.x),component=magnitude(n.y);
    if(component>maximum)maximum=component;
    component=magnitude(n.z);if(component>maximum)maximum=component;
    unsigned shift=0;
    while(maximum>CLOWN_NORMAL_LIMIT){maximum>>=1;++shift;}
    n.x>>=shift;n.y>>=shift;n.z>>=shift;
    /* Arithmetic shifts round negative components down, so their magnitude
     * may need one more bit than the unsigned estimate. */
    if(magnitude(n.x)>CLOWN_NORMAL_LIMIT || magnitude(n.y)>CLOWN_NORMAL_LIMIT ||
       magnitude(n.z)>CLOWN_NORMAL_LIMIT){n.x>>=1;n.y>>=1;n.z>>=1;}
    uint32_t length=sat_isqrt32((uint32_t)(n.x*n.x+n.y*n.y+n.z*n.z));
    if(!length)return (sat_vec3_t){0,0,0};
    if(!normal_reciprocals_ready) {
        for(unsigned i=1;i<NORMAL_RECIP_CAPACITY;++i)normal_reciprocals[i]=UINT32_MAX/i;
        normal_reciprocals_ready=1;
    }
    return (sat_vec3_t){normal_component(n.x,length),normal_component(n.y,length),normal_component(n.z,length)};
}
void clown_spotlight_aim(clown_spotlight_t* light,sat_vec3_t source,sat_vec3_t target) {
    light->source=source;light->target=target;
    light->axis=clown_light_normalize((sat_vec3_t){target.x-source.x,target.y-source.y,target.z-source.z});
    light->u=clown_light_normalize((sat_vec3_t){-light->axis.z,0,light->axis.x});
    if(!light->u.x && !light->u.z)light->u=(sat_vec3_t){SAT_FX16_ONE,0,0};
    sat_vec3_cross(&light->v,&light->axis,&light->u);
}
unsigned clown_spotlight_strength(const clown_spotlight_t* light,sat_vec3_t point) {
    int32_t x=(point.x-light->source.x)>>CLOWN_LIGHT_SHIFT;
    int32_t y=(point.y-light->source.y)>>CLOWN_LIGHT_SHIFT;
    int32_t z=(point.z-light->source.z)>>CLOWN_LIGHT_SHIFT;
    uint32_t distance=(uint32_t)(x*x+y*y+z*z);
    const uint32_t range=CLOWN_LIGHT_RANGE_Q8*CLOWN_LIGHT_RANGE_Q8;
    if(distance>=range)return 0;
    int32_t depth=(x*(light->axis.x>>CLOWN_LIGHT_SHIFT)+y*(light->axis.y>>CLOWN_LIGHT_SHIFT)+
                   z*(light->axis.z>>CLOWN_LIGHT_SHIFT))>>CLOWN_LIGHT_SHIFT;
    if(depth<=0)return 0;
    int32_t radial=(int32_t)distance-depth*depth;if(radial<0)radial=0;
    uint32_t radius=(uint32_t)depth*CLOWN_CONE_TAN_Q8/CLOWN_LIGHT_Q8;
    uint32_t outer=radius*radius,inner=outer/4;
    if(!outer || (uint32_t)radial>=outer)return 0;
    unsigned cone=(uint32_t)radial<=inner?(unsigned)CLOWN_LIGHT_Q8:
        (outer-(uint32_t)radial)*CLOWN_LIGHT_Q8/(outer-inner);
    unsigned attenuation=(range-distance)*CLOWN_LIGHT_Q8/range;
    return cone*attenuation/CLOWN_LIGHT_Q8;
}
unsigned clown_spotlight_diffuse(const clown_spotlight_t* light,sat_vec3_t point,sat_vec3_t normal) {
    unsigned strength=clown_spotlight_strength(light,point);
    if(!strength)return 0;
    sat_vec3_t direction=clown_light_normalize((sat_vec3_t){light->source.x-point.x,
        light->source.y-point.y,light->source.z-point.z});
    int32_t dot=(normal.x>>CLOWN_LIGHT_SHIFT)*(direction.x>>CLOWN_LIGHT_SHIFT)+
        (normal.y>>CLOWN_LIGHT_SHIFT)*(direction.y>>CLOWN_LIGHT_SHIFT)+
        (normal.z>>CLOWN_LIGHT_SHIFT)*(direction.z>>CLOWN_LIGHT_SHIFT);
    if(dot<=0)return 0;
    if(dot>SAT_FX16_ONE)dot=SAT_FX16_ONE;
    return strength*(unsigned)dot/SAT_FX16_ONE;
}
int clown_spotlight_ground(const clown_spotlight_t* light,sat_fx16_t angle,sat_vec3_t* point) {
    sat_fx16_t cosine=sat_cos_deg(angle),sine=sat_sin_deg(angle);
    sat_fx16_t coordinates[3];
    const sat_fx16_t axis[3]={light->axis.x,light->axis.y,light->axis.z};
    const sat_fx16_t u[3]={light->u.x,light->u.y,light->u.z},v[3]={light->v.x,light->v.y,light->v.z};
    for(unsigned channel=0;channel<3;++channel)
        coordinates[channel]=axis[channel]+sat_fx16_mul(
            sat_fx16_mul(u[channel],cosine)+sat_fx16_mul(v[channel],sine),CLOWN_CONE_TAN_Q8*CLOWN_LIGHT_Q8);
    sat_vec3_t ray={coordinates[0],coordinates[1],coordinates[2]};
    sat_fx16_t length=CLOWN_LIGHT_RANGE_Q8*CLOWN_LIGHT_Q8;
    int hit=0;
    if(ray.y<0) {
        sat_fx16_t intersection=sat_fx16_div(-light->source.y,ray.y);
        if(intersection<length){length=intersection;hit=1;}
    }
    point->x=light->source.x+sat_fx16_mul(ray.x,length);
    point->y=hit?0:light->source.y+sat_fx16_mul(ray.y,length);
    point->z=light->source.z+sat_fx16_mul(ray.z,length);
    return hit;
}
