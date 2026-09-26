#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/hal/sh2/dmac_logic.hpp"

using namespace saturn::hal::sh2::dmac_logic;

static void copies_between_work_rams_plan_longword_units() {
    Plan p{};
    assert(plan_copy(0x06010000u, 0x06020000u, 1024u, &p) == Status::Ok);
    assert(p.units == 256u && p.chcr == 0x5A11u);
    /* Work RAM-L is reachable to the DMAC even though the SCU refuses it. */
    assert(plan_copy(0x06010000u, 0x00200000u, 4096u, &p) == Status::Ok);
    assert(plan_copy(0x00200000u, 0x06010000u, 128u, &p) == Status::Ok && p.units == 32u);
}

static void chcr_bits_follow_the_manual() {
    /* DM=01, SM=01, TS=10, AR, TB, DE; no interrupt enable. */
    assert(kChcrLongCopy == ((1u << 14) | (1u << 12) | (2u << 10) | (1u << 9) | (1u << 4) | 1u));
    assert((kChcrLongCopy & 0x0004u) == 0u);
}

static void rejects_what_the_controller_should_not_be_given() {
    Plan p{};
    assert(plan_copy(0x06010000u, 0x06020000u, 64u, &p) == Status::BadSize);   /* too small */
    assert(plan_copy(0x06010000u, 0x06020000u, 130u, &p) == Status::BadSize);  /* not longwords */
    assert(plan_copy(0x06010000u, 0x06020000u, kMaxBytes + 4u, &p) == Status::BadSize);
    assert(plan_copy(0x06010002u, 0x06020000u, 256u, &p) == Status::Misaligned);
    assert(plan_copy(0x06010000u, 0x06020002u, 256u, &p) == Status::Misaligned);
    assert(plan_copy(0x06010000u, 0x05C00000u, 256u, &p) == Status::NotRam);   /* VDP1: SCU's job */
    assert(plan_copy(0x05E00000u, 0x06010000u, 256u, &p) == Status::NotRam);
    assert(plan_copy(0x0600FFC0u, 0x0600FFF0u + 0x80u, 256u, &p) == Status::Overlap);
}

static void overlap_matters_only_for_a_forward_copy() {
    assert(forward_overlap(0x06000000u, 0x06000040u, 0x100u));
    assert(!forward_overlap(0x06000040u, 0x06000000u, 0x100u)); /* moving down is safe */
    assert(!forward_overlap(0x06000000u, 0x06000100u, 0x100u)); /* adjacent, disjoint */
    assert(!forward_overlap(0x06000000u, 0x06000000u, 0x100u)); /* same buffer */
}

int main() {
    copies_between_work_rams_plan_longword_units();
    chcr_bits_follow_the_manual();
    rejects_what_the_controller_should_not_be_given();
    overlap_matters_only_for_a_forward_copy();
    std::printf("PASS: test_sh2_dmac_logic.cpp\n");
    return 0;
}
