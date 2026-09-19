#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/core/scene3d_queue_logic.hpp"

using saturn::core::scene3d_queue::before;
using saturn::core::scene3d_queue::sort;

int main() {
    /* Include negative, positive and equal depth values, mixed passes
     * and reverse/incoherent insertion order to test the full heap sort,
     * not only the tiny five-item queue exercised by the public facade. */
    sat_scene3d_queue_item_t items[256]={};
    for(uint16_t i=0;i<256;++i) {
        items[i].pass=(uint16_t)((i*13u+5u)%4u);
        items[i].depth=(int64_t)((i*37u+19u)%23u)-11;
        items[i].submission=i;
    }
    sort(items,256);
    for(uint16_t i=1;i<256;++i) {
        assert(!before(items[i],items[i-1u]));
        if(items[i].pass==items[i-1u].pass &&
           items[i].depth==items[i-1u].depth)
            assert(items[i].submission>items[i-1u].submission);
    }
    /* Re-sorting an already sorted queue must not change ordering. */
    uint16_t saved[256];
    for(uint16_t i=0;i<256;++i)saved[i]=items[i].submission;
    sort(items,256);
    for(uint16_t i=0;i<256;++i)assert(items[i].submission==saved[i]);
    sort(nullptr,0u);
    sort(items,0u);
    sort(items,1u);
    std::cout<<"scene3d queue heap order: 256 mixed-pass depth entries OK\n";
    return 0;
}
