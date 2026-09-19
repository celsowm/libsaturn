#ifndef LIBSATURN_SKYBRIDGE_SCENERY_H
#define LIBSATURN_SKYBRIDGE_SCENERY_H

#include <stdint.h>

/* Allocation-free procedural scenery. Pixel indices are palette contracts:
 * sky gradient 1..128, hills 168..171, haze 176..179;
 * sea base 1..47, glints 48..55, foam 56..63;
 * cloud 0=transparent and 1..4=shaded cloud pixels. */
#define SB_SKY_W 512u
#define SB_SKY_H 128u
#define SB_SEA_W 512u
#define SB_SEA_H 256u
#define SB_CLOUD_W 64u
#define SB_CLOUD_H 16u

static inline uint8_t sb_scenery_min31(uint32_t n) {
    return (uint8_t)(n > 31u ? 31u : n);
}
static inline uint16_t sb_scenery_rgb(uint32_t r,uint32_t g,uint32_t b) {
    return (uint16_t)((sb_scenery_min31(b)<<10u) |
                      (sb_scenery_min31(g)<<5u) | sb_scenery_min31(r));
}
static inline uint32_t sb_scenery_wrap512(int32_t n) {
    return (uint32_t)n & 511u;
}
static inline int32_t sb_scenery_cloud_x(uint16_t base_x,uint16_t yaw,
                                         uint32_t tick) {
    /* Same angular camera tracking as NBG0, but wind is 2x quicker
     * than the distant island silhouette: independent cloud parallax. */
    uint32_t camera=((uint32_t)yaw*SB_SKY_W)/360u;
    uint32_t wind=(tick>>5u);
    return (int32_t)sb_scenery_wrap512((int32_t)base_x-(int32_t)camera-
                                       (int32_t)wind);
}
static inline uint16_t sb_scenery_sky_scroll(uint16_t yaw,uint32_t tick) {
    return (uint16_t)sb_scenery_wrap512(
        (int32_t)(((uint32_t)yaw*SB_SKY_W)/360u)+(int32_t)(tick>>7u));
}
/* Two low-frequency closed island profiles with deliberate, non-periodic
 * peaks. Linear interpolation avoids the old picket-fence XOR silhouette. */
static inline uint8_t sb_scenery_island_height(uint32_t x,uint8_t near) {
    static const uint8_t far_profile[17] = {
        123u,119u,115u,118u,124u,125u,121u,113u,109u,
        115u,122u,124u,119u,116u,120u,124u,123u
    };
    static const uint8_t near_profile[17] = {
        127u,125u,121u,123u,127u,128u,126u,122u,119u,
        122u,127u,128u,125u,123u,126u,128u,127u
    };
    const uint8_t* p=near?near_profile:far_profile;
    uint32_t xx=x&511u, cell=xx>>5u, frac=xx&31u;
    return (uint8_t)(((uint32_t)p[cell]*(32u-frac)+
                      (uint32_t)p[cell+1u]*frac+16u)>>5u);
}
static inline uint8_t sb_scenery_sky_pixel(uint32_t x,uint32_t y) {
    uint8_t index=(uint8_t)(1u+(y*112u)/127u);
    uint8_t far_h=sb_scenery_island_height(x,0u);
    uint8_t near_h=sb_scenery_island_height(x,1u);
    /* Gradual atmospheric strip is ABOVE the hard ocean/sky line (screen
     * y=96, NBG0 source row 127 at its fixed +31 vertical offset). */
    if(y>=104u) index=(uint8_t)(176u+((y-104u)>>3u));
    if(y>=far_h) index=(uint8_t)(168u+((x>>6u)&1u));
    if(y>=near_h) index=(uint8_t)(170u+((x>>5u)&1u));
    return index;
}
static inline uint8_t sb_scenery_cloud_pixel(uint32_t x,uint32_t y) {
    /* One shaded, transparent 64x16 cloud built from three overlapping
     * ellipses. Avoid integer divisions in the per-pixel loop. */
    static const uint8_t cx[3]={17u,33u,48u};
    static const uint8_t cy[3]={9u,6u,10u};
    static const uint8_t rx[3]={17u,21u,15u};
    static const uint8_t ry[3]={6u,6u,5u};
    uint8_t i,mask=0u;
    for(i=0u;i<3u;++i) {
        int32_t dx=(int32_t)x-(int32_t)cx[i];
        int32_t dy=(int32_t)y-(int32_t)cy[i];
        int32_t a=(int32_t)rx[i]*rx[i], b=(int32_t)ry[i]*ry[i];
        if(dx*dx*b+dy*dy*a<=a*b) mask=1u;
    }
    if(!mask)return 0u;
    if(y<=4u)return 1u;          /* bright sunlit tops */
    if(y>=12u)return 4u;         /* subtle blue-gray underside */
    if(y>=9u)return 3u;
    return 2u;
}
static inline uint8_t sb_scenery_sea_pixel(uint32_t x,uint32_t y) {
    /* Spacious staggered ripples; no XOR / high-frequency noise. The
     * RBG0 coefficient table handles perspective, so these are WORLD
     * texture coordinates, not screen scanlines or a screen-depth ramp. */
    uint32_t band=(y+((x>>5u)&7u)+((x>>7u)&3u))%37u;
    uint32_t drift=(x+(y>>2u)*11u+(y>>5u)*7u)&127u;
    uint32_t base=12u+(y>>6u)*2u+((x>>5u)&3u);
    if(band==0u && drift<69u)return (uint8_t)(56u+((x>>5u)&7u));
    if(band<=2u && drift<87u)return (uint8_t)(48u+((x>>6u)&7u));
    /* Dark gaps add contrast without per-pixel/static grain. */
    if((band>=17u && band<=20u) && drift<55u)base-=3u;
    return (uint8_t)base;
}
static inline uint16_t sb_scenery_sea_color(uint32_t i) {
    if(i==0u)return 0u;       /* index 0 stays transparent */
    if(i<48u) {
        uint32_t t=i-1u;
        return sb_scenery_rgb(2u+t/19u,7u+t/4u,15u+t/4u);
    }
    if(i<56u) {
        uint32_t t=i-48u;
        return sb_scenery_rgb(6u+t/4u,19u+t/2u,24u+t/3u);
    }
    {
        uint32_t t=i-56u;
        return sb_scenery_rgb(11u+t/3u,25u+t/4u,28u+t/4u);
    }
}
static inline uint16_t sb_scenery_sky_color(uint32_t i) {
    if(i>=1u && i<=128u) {
        uint32_t t=i-1u;
        return sb_scenery_rgb(5u+t*7u/127u,14u+t*10u/127u,
                              24u+t*5u/127u);
    }
    if(i>=168u && i<=169u)
        return sb_scenery_rgb(15u,21u-(i-168u),25u-(i-168u));
    if(i>=170u && i<=171u)
        return sb_scenery_rgb(11u,18u-(i-170u),22u-(i-170u));
    if(i>=176u && i<=179u) {
        uint32_t t=i-176u;
        return sb_scenery_rgb(13u+t*2u,23u+t,29u+t/2u);
    }
    return sb_scenery_rgb(7u,17u,25u);
}
#endif /* LIBSATURN_SKYBRIDGE_SCENERY_H */
