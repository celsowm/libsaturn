#include <cassert>
#include <cstdint>
#include <iostream>

#include "examples/skybridge_3d/scenery.h"

int main() {
    /* A 512x128 sky panorama must tile at x=0/512 with no skyline seam. */
    for (uint32_t y=0;y<SB_SKY_H;++y) {
        assert(sb_scenery_sky_pixel(0u,y)==sb_scenery_sky_pixel(512u,y));
        assert(sb_scenery_sky_pixel(1u,y)==sb_scenery_sky_pixel(513u,y));
    }
    assert(sb_scenery_island_height(0u,0u)==sb_scenery_island_height(512u,0u));
    assert(sb_scenery_island_height(0u,1u)==sb_scenery_island_height(512u,1u));
    uint32_t hills=0u,sky=0u,foam=0u,glints=0u,deep=0u;
    for(uint32_t y=0u;y<SB_SKY_H;++y)for(uint32_t x=0u;x<SB_SKY_W;++x) {
        uint8_t idx=sb_scenery_sky_pixel(x,y);
        assert((idx>=1u&&idx<=128u)||(idx>=168u&&idx<=171u)||
               (idx>=176u&&idx<=179u));
        if(idx>=168u&&idx<=171u)++hills;
        if(idx>=1u&&idx<=128u)++sky;
    }
    assert(hills>0u&&sky>0u);
    for(uint32_t y=0u;y<SB_SEA_H;++y)for(uint32_t x=0u;x<SB_SEA_W;++x) {
        uint8_t idx=sb_scenery_sea_pixel(x,y);
        assert(idx>=1u&&idx<64u);
        if(idx>=56u)++foam;
        else if(idx>=48u)++glints;
        else ++deep;
    }
    assert(foam>0u&&glints>0u&&deep>0u);
    assert(sb_scenery_sea_color(0u)==0u);
    assert(sb_scenery_sea_color(63u)!=sb_scenery_sea_color(1u));
    for(uint32_t i=0u;i<256u;++i) {
        assert((sb_scenery_sea_color(i&63u)&0x8000u)==0u);
        assert((sb_scenery_sky_color(i)&0x8000u)==0u);
    }
    uint32_t clouds=0u,transparent=0u;
    for(uint32_t y=0u;y<SB_CLOUD_H;++y)for(uint32_t x=0u;x<SB_CLOUD_W;++x) {
        uint8_t idx=sb_scenery_cloud_pixel(x,y);
        assert(idx<=4u);
        if(idx==0u)++transparent;
        else ++clouds;
    }
    assert(clouds>0u&&transparent>0u);
    assert(sb_scenery_cloud_pixel(0u,0u)==0u);
    assert(sb_scenery_cloud_pixel(33u,6u)>0u);
    /* NBG0 islands drift at 1px/128 frames; VDP1 cloud sprites
       drift at 1px/32 frames (nonidentical rates, independent layers). */
    assert(sb_scenery_sky_scroll(0u,0u)==0u);
    assert(sb_scenery_sky_scroll(0u,128u)==1u);
    assert(sb_scenery_cloud_x(100u,0u,0u)==100);
    assert(sb_scenery_cloud_x(100u,0u,128u)==96);
    assert(sb_scenery_cloud_x(0u,0u,0u)==0);
    assert(sb_scenery_cloud_x(0u,0u,32u)==511);
    assert(sb_scenery_sky_scroll(0u,128u)==
           sb_scenery_sky_scroll(360u,128u));

    std::cout<<"skybridge scenery: sea, horizon, cloud and parallax tests passed\n";
    return 0;
}
