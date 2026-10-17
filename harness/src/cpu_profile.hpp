// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace harness {
// Emulator-independent accounting. Cycles are execution-step return values,
// never deltas of the fixed system/VBlank clock. Interrupt entry is separate.
struct CpuProfile {
    struct Cost { uint64_t instructions=0, cycles=0; };
    struct Event {
        uint32_t frame, pc, target;
        uint64_t instructions, cycles;
        const char* kind;
    };
    bool enabled=false, active=false;
    uint32_t frame=0;
    size_t capacity=65536;
    uint64_t instructions=0, cycles=0, interrupt_cycles=0;
    uint64_t dropped_pcs=0, dropped_events=0;
    uint64_t calls=0, returns=0, interrupts=0, divisions=0, dma_transfers=0;
    std::map<uint32_t,Cost> pcs;
    std::vector<Event> events;

    void complete(uint32_t pc, uint64_t cost, bool instruction) {
        if (!active) return;
        cycles+=cost;
        if (!instruction) { interrupt_cycles+=cost; return; }
        ++instructions;
        auto it=pcs.find(pc);
        if (it==pcs.end()) {
            if (pcs.size()>=capacity) { ++dropped_pcs; return; }
            it=pcs.emplace(pc,Cost{}).first;
        }
        ++it->second.instructions;
        it->second.cycles+=cost;
    }
    void event(const char* kind,uint32_t pc,uint32_t target) {
        if (!active) return;
        if (events.size()>=capacity) { ++dropped_events; return; }
        events.push_back({frame,pc,target,instructions,cycles,kind});
    }
    bool truncated() const { return dropped_pcs || dropped_events; }
};
} // namespace harness
