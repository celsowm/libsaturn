#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/hal/scu/dma_logic.hpp"

using namespace saturn::hal::scu::dma_logic;

static void aliases_share_one_physical_location() {
    assert(physical(0x06004000u) == 0x06004000u);
    assert(physical(0x26004000u) == 0x06004000u);
    assert(physical(0x25C00000u) == 0x05C00000u);
    assert(classify(physical(0x25C00000u)) == Region::Vdp1);
    assert(classify(physical(0x25E00000u)) == Region::Vdp2);
    assert(classify(physical(0x25F00000u)) == Region::Vdp2);  /* CRAM */
    assert(classify(physical(0x25A00000u)) == Region::Scsp);
    assert(classify(physical(0x25B00400u)) == Region::Other); /* SCSP registers */
    assert(classify(0x00200000u) == Region::WorkRamL);
    assert(classify(0x02000000u) == Region::ABus);
    assert(classify(0x05D00000u) == Region::Other);            /* VDP1 registers */
}

static void spans_may_not_cross_regions() {
    assert(classify_span(0x05C7FFF0u, 0x20u) == Region::Vdp1);  /* into the frame buffer */
    assert(classify_span(0x05CBFFF0u, 0x20u) == Region::Other);
    assert(classify_span(0x06000000u, 0x100000u) == Region::WorkRamH);
    assert(classify_span(0x07FFFFF0u, 0x20u) == Region::Other);
    assert(classify_span(0x06000000u, 0u) == Region::Other);
}

static void routes_follow_the_manual() {
    assert(route_allowed(Region::WorkRamH, Region::Vdp1));
    assert(route_allowed(Region::WorkRamH, Region::Vdp2));
    assert(route_allowed(Region::WorkRamH, Region::Scsp));
    assert(route_allowed(Region::Vdp1, Region::WorkRamH));
    assert(route_allowed(Region::ABus, Region::WorkRamH));
    assert(route_allowed(Region::ABus, Region::Vdp2));
    /* Forbidden: A-bus writes, VDP2 as a source, Work RAM-L, RAM to RAM,
     * B-bus to B-bus. */
    assert(!route_allowed(Region::WorkRamH, Region::ABus));
    assert(!route_allowed(Region::Vdp2, Region::WorkRamH));
    assert(!route_allowed(Region::WorkRamL, Region::Vdp1));
    assert(!route_allowed(Region::WorkRamH, Region::WorkRamL));
    assert(!route_allowed(Region::WorkRamH, Region::WorkRamH));
    assert(!route_allowed(Region::Vdp1, Region::Vdp2));
    assert(!route_allowed(Region::Other, Region::Vdp1));
}

static void direct_plan_encodes_the_registers() {
    Plan p{};
    const Transfer to_vram{0x06010000u, 0x05C00000u, 0x400u};
    assert(plan_direct(0u, to_vram, &p) == Status::Ok);
    assert(p.read == 0x06010000u && p.write == 0x05C00000u);
    assert(p.count == 0x400u && p.add == 0x101u && p.mode == 7u);

    const Transfer from_vram{0x05C00000u, 0x06010000u, 0x100u};
    assert(plan_direct(1u, from_vram, &p) == Status::Ok);
    assert(p.add == 0x102u);  /* Work RAM is written 4 bytes at a time */

    /* The level maximum is written as 0 (D0C is 20 bits, D1C/D2C 12). */
    const Transfer full0{0x06000000u, 0x05E00000u, 0x100000u};
    assert(plan_direct(0u, full0, &p) == Status::Ok && p.count == 0u);
    const Transfer full1{0x06000000u, 0x05E00000u, 0x1000u};
    assert(plan_direct(2u, full1, &p) == Status::Ok && p.count == 0u);
}

static void direct_plan_rejects_what_the_hardware_cannot_do() {
    Plan p{};
    assert(plan_direct(3u, {0x06000000u, 0x05C00000u, 4u}, &p) == Status::BadLevel);
    assert(plan_direct(0u, {0x06000000u, 0x05C00000u, 0u}, &p) == Status::BadSize);
    assert(plan_direct(0u, {0x06000000u, 0x05C00000u, 6u}, &p) == Status::BadSize);
    assert(plan_direct(1u, {0x06000000u, 0x05C00000u, 0x1004u}, &p) == Status::BadSize);
    assert(plan_direct(0u, {0x06000001u, 0x05C00000u, 4u}, &p) == Status::Misaligned);
    assert(plan_direct(0u, {0x06000000u, 0x05C00002u, 4u}, &p) == Status::Misaligned);
    assert(plan_direct(0u, {0x06000000u, 0x02000000u, 4u}, &p) == Status::IllegalRoute);
    assert(plan_direct(0u, {0x05E00000u, 0x06000000u, 4u}, &p) == Status::IllegalRoute);
    assert(plan_direct(0u, {0x00200000u, 0x05C00000u, 4u}, &p) == Status::IllegalRoute);
}

static void indirect_table_ends_on_the_last_entry() {
    const Transfer list[3] = {
        {0x06000100u, 0x05C00000u, 0x20u},
        {0x06000200u, 0x05C01000u, 0x10u},
        {0x06000300u, 0x05C02000u, 0x1000u},
    };
    uint32_t words[3 * kWordsPerEntry] = {};
    uint32_t add = 0u;
    Region first = Region::Other;
    assert(build_indirect(1u, list, 3u, words, &add, &first) == Status::Ok);
    assert(add == 0x101u && first == Region::Vdp1);
    /* count, write, read -- as in the manual's figure 2.8 */
    assert(words[0] == 0x20u && words[1] == 0x05C00000u && words[2] == 0x06000100u);
    assert(words[3] == 0x10u && words[4] == 0x05C01000u && words[5] == 0x06000200u);
    assert(words[6] == 0u);                          /* 0x1000 is level 1's maximum */
    assert(words[8] == (0x06000300u | 0x80000000u)); /* end code on the last read */
    assert((words[2] & 0x80000000u) == 0u);
}

static void indirect_lists_are_checked_as_a_whole() {
    uint32_t words[kMaxListEntries * kWordsPerEntry] = {};
    uint32_t add = 0u;
    Transfer many[kMaxListEntries + 1u];
    for (auto& t : many) t = {0x06000000u, 0x05C00000u, 4u};
    assert(build_indirect(0u, many, kMaxListEntries, words, &add, nullptr) == Status::Ok);
    assert(build_indirect(0u, many, kMaxListEntries + 1u, words, &add, nullptr) ==
           Status::BadList);
    assert(build_indirect(0u, many, 0u, words, &add, nullptr) == Status::BadList);
    assert(build_indirect(0u, nullptr, 1u, words, &add, nullptr) == Status::BadList);
    /* One DxAD serves the list: VRAM (2-byte writes) and Work RAM (4) differ. */
    const Transfer mixed[2] = {{0x06000000u, 0x05C00000u, 4u},
                               {0x05C00000u, 0x06000100u, 4u}};
    assert(build_indirect(0u, mixed, 2u, words, &add, nullptr) == Status::BadList);
    /* An illegal entry names its own status. */
    const Transfer bad[2] = {{0x06000000u, 0x05C00000u, 4u},
                             {0x06000000u, 0x02000000u, 4u}};
    assert(build_indirect(0u, bad, 2u, words, &add, nullptr) == Status::IllegalRoute);
    assert(build_indirect(3u, bad, 1u, words, &add, nullptr) == Status::BadLevel);
}

static void register_addresses_follow_the_manual() {
    assert(level_register(0u, kOffsetRead) == 0x25FE0000u);
    assert(level_register(1u, kOffsetWrite) == 0x25FE0024u);
    assert(level_register(2u, kOffsetMode) == 0x25FE0054u);
    assert(level_register(1u, kOffsetEnable) == 0x25FE0030u);
    assert(kStatusBusy[0] == 0x30u && kStatusBusy[2] == 0x3000u);
    assert(kIstEnd[0] == 0x800u && kIstEnd[2] == 0x200u && kIstIllegal == 0x1000u);
}

int main() {
    aliases_share_one_physical_location();
    spans_may_not_cross_regions();
    routes_follow_the_manual();
    direct_plan_encodes_the_registers();
    direct_plan_rejects_what_the_hardware_cannot_do();
    indirect_table_ends_on_the_last_entry();
    indirect_lists_are_checked_as_a_whole();
    register_addresses_follow_the_manual();
    std::printf("PASS: test_scu_dma_logic.cpp\n");
    return 0;
}
