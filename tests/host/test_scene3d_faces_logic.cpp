#include <cassert>
#include <cstdio>
#include "src/core/scene3d_faces_logic.hpp"

int main() {
    sat_scene3d_face_t items[6]={};
    // One platform has one face BEHIND and another IN FRONT of the actor.
    // Sorting object anchors cannot produce this interleaving.
    items[0].depth=10;items[0].sequence=0; // platform far face
    items[1].depth=2; items[1].sequence=1; // platform near face
    items[2].depth=6; items[2].sequence=2; // pig
    items[3].depth=7; items[3].sequence=3; // gem
    items[4].depth=7; items[4].sequence=4; // equal-depth gem
    items[5].depth=30;items[5].sequence=5;items[5].pass=1; // HUD
    saturn::core::scene3d_faces::sort(items,6);
    assert(items[0].sequence==0);
    assert(items[1].sequence==3 && items[2].sequence==4);
    assert(items[3].sequence==2 && items[4].sequence==1);
    assert(items[5].sequence==5);
    items[0].pass=0; items[0].depth=3;items[0].sequence=0;
    items[1].pass=0; items[1].depth=11;items[1].sequence=1;
    saturn::core::scene3d_faces::sort(items,2);
    assert(items[0].sequence==1 && items[1].sequence==0);
    saturn::core::scene3d_faces::sort(nullptr,0);
    std::puts("scene-wide face painter: OK");
    return 0;
}
