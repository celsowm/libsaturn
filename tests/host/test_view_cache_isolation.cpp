#include <cassert>
#include <cstdio>

#include "saturn/view_cache.h"

/* Linking this executable against view_cache.cpp alone must not require
 * the world projector or any other renderer implementation. */
int main() {
    sat_view_cache_item_t storage[1]={};
    uint16_t counts[1]={};
    sat_view_cache_t cache{};
    sat_quad2_t quad{};
    const sat_view_cache_item_t* out=nullptr;
    uint16_t count=0u;
    assert(sat_view_cache_init(&cache,storage,counts,1u,1u)==SAT_OK);
    assert(sat_view_cache_begin(&cache,0u)==SAT_OK);
    assert(sat_view_cache_append(&cache,&quad,0xFFFFFFFFu,5u,0u)==SAT_OK);
    assert(sat_view_cache_sort(&cache)==SAT_OK);
    assert(sat_view_cache_view(&cache,0u,&out,&count)==SAT_OK);
    assert(out!=nullptr && count==1u && out[0].camera_depth_valid==0u);
    std::puts("view cache L1 link isolation: OK");
    return 0;
}
