// SPDX-License-Identifier: GPL-3.0-only
#include "cpu_profile.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool ok) { if (!ok) { std::cerr<<"CPU profile check failed\n"; std::exit(1); } }
int main() {
    harness::CpuProfile m,s;
    m.complete(0x100,9,true); check(m.cycles==0 && m.pcs.empty());
    m.active=true; m.capacity=2;
    m.complete(0x100,3,true); m.complete(0x100,4,true);
    m.complete(0x100,11,false); // interrupt entry is not an instruction
    m.complete(0x102,2,true);
    check(m.instructions==3 && m.cycles==20 && m.interrupt_cycles==11);
    check(m.pcs.at(0x100).instructions==2 && m.pcs.at(0x100).cycles==7);
    m.complete(0x104,8,true);
    check(m.instructions==4 && m.cycles==28 && m.dropped_pcs==1 && m.pcs.size()==2);
    m.frame=123; m.event("call",0x100,0x200); m.event("return",0x200,0x102);
    m.event("call",0x100,0x300);
    check(m.events.size()==2 && m.dropped_events==1 && m.truncated());
    check(m.events[0].frame==123 && m.events[0].cycles==28);
    m.active=false; m.complete(0x100,30,true); m.event("return",0x200,0x102);
    check(m.cycles==28 && m.events.size()==2);
    s.active=true; s.complete(0x100,17,true);
    check(s.cycles==17 && m.pcs.at(0x100).cycles==7);
    std::cout<<"CPU profile accounting passed\n";
}
