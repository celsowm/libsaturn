/* test_vdp2_plan_logic.cpp — host tests for the pure VDP2 resource-plan
 * policy (src/hal/vdp2/resource_plan_logic.hpp). No linkage beyond the
 * header: the state is built on the stack, so every case starts clean.
 * The runtime behaviour against the real free list is covered by
 * test_vdp2_plan_api.cpp. */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "src/hal/vdp2/resource_plan_logic.hpp"

namespace plan = saturn::hal::vdp2::plan;

#define ASSERT_EQ(a, b) do { auto __ae = (a); auto __be = (b); \
    if (__ae != __be) { \
    fprintf(stderr, "FAIL %s:%d: %s=%lld != %s=%lld\n", __FILE__, __LINE__, \
        #a, (long long)__ae, #b, (long long)__be); \
    exit(1); } } while (0)
#define ASSERT_TRUE(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
    exit(1); } } while (0)

static void test_transaction_lifecycle(void) {
    plan::PlanState s;
    plan::init(s);
    ASSERT_EQ(static_cast<int>(plan::commit(s)), static_cast<int>(plan::Status::NotOpen));
    ASSERT_EQ(static_cast<int>(plan::rollback(s, nullptr, nullptr)),
              static_cast<int>(plan::Status::NotOpen));
    ASSERT_EQ(static_cast<int>(plan::open(s)), static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::open(s)),
              static_cast<int>(plan::Status::AlreadyOpen));
    ASSERT_EQ(static_cast<int>(plan::commit(s)), static_cast<int>(plan::Status::Ok));
    ASSERT_TRUE(!s.open);
    /* Rollback after commit: the transaction is closed. */
    ASSERT_EQ(static_cast<int>(plan::rollback(s, nullptr, nullptr)),
              static_cast<int>(plan::Status::NotOpen));
}

static void test_kind_minimum_alignments(void) {
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    /* Rotation parameter tables are 4-byte aligned (manual 6.3, the
     * runtime checks a multiple of 4 in layers.cpp). */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationParams,
                                           0x00002u, 0x60u, 4u)),
              static_cast<int>(plan::Status::BadAlign));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationParams,
                                           0x00004u, 0x60u, 4u)),
              static_cast<int>(plan::Status::Ok));
    plan::init(s);
    plan::open(s);
    /* Bitmaps are 0x20000-aligned (one bitmap per 128 KiB bank). */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Bitmap,
                                           0x00002u, 0x40000u, 0x20000u)),
              static_cast<int>(plan::Status::BadAlign));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Bitmap,
                                           0x20000u, 0x40000u, 0x20000u)),
              static_cast<int>(plan::Status::Ok));
    plan::init(s);
    plan::open(s);
    /* Line scroll tables need an even address only. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::LineScroll,
                                           0x00001u, 0x100u, 2u)),
              static_cast<int>(plan::Status::BadAlign));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::LineScroll,
                                           0x00002u, 0x100u, 2u)),
              static_cast<int>(plan::Status::Ok));
    plan::init(s);
    plan::open(s);
    /* A caller may ask for a stricter alignment, never a weaker one. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::WindowTable,
                                           0x00010u, 0x10u, 0x10u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::WindowTable,
                                           0x00010u, 0x10u, 2u)),
              static_cast<int>(plan::Status::BadAlign));
}

static void test_range_rules(void) {
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x00000u, 0u, 2u)),
              static_cast<int>(plan::Status::BadRange));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x80000u, 0x10u, 2u)),
              static_cast<int>(plan::Status::BadRange));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x7FFE0u, 0x20u, 2u)),
              static_cast<int>(plan::Status::Ok));
    plan::init(s);
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x7FFF0u, 0x11u, 2u)),
              static_cast<int>(plan::Status::BadRange));   /* wraps past VRAM */
    ASSERT_EQ(static_cast<int>(plan::stage(s, static_cast<plan::Kind>(99u),
                                           0x0u, 0x10u, 2u)),
              static_cast<int>(plan::Status::BadKind));
}

static void test_overlap_detection(void) {
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Characters,
                                           0x10000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::Ok));
    /* Adjacent ranges do not overlap. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Characters,
                                           0x12000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::Ok));
    /* Two bytes inside the first range do. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::PatternNames,
                                           0x11FFEu, 0x10u, 2u)),
              static_cast<int>(plan::Status::Overlap));
    ASSERT_EQ(s.last_conflict_base, 0x10000u);
    /* Contained ranges overlap too. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x10100u, 0x100u, 2u)),
              static_cast<int>(plan::Status::Overlap));
    ASSERT_EQ(s.refusals, 2u);
}

static void test_bank_roles(void) {
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::claim_banks(s, 0x01u)),   /* A0 */
              static_cast<int>(plan::Status::Ok));
    /* NBG/rotation kinds may not live in the claimed bank. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::PatternNames,
                                           0x00000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::BankClaimed));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Characters,
                                           0x10000u, 0x8000u, 2u)),
              static_cast<int>(plan::Status::BankClaimed));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Bitmap,
                                           0x00000u, 0x20000u, 0x20000u)),
              static_cast<int>(plan::Status::BankClaimed));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationParams,
                                           0x00004u, 0x60u, 4u)),
              static_cast<int>(plan::Status::BankClaimed));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Coefficients,
                                           0x00100u, 0x400u, 4u)),
              static_cast<int>(plan::Status::BankClaimed));
    /* Tables that need no cycle pattern may. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::LineScroll,
                                           0x00010u, 0x200u, 2u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::WindowTable,
                                           0x00800u, 0x400u, 4u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::BackScreen,
                                           0x01000u, 0x200u, 2u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::LineColor,
                                           0x01400u, 0x200u, 2u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x01800u, 0x800u, 2u)),
              static_cast<int>(plan::Status::Ok));
    /* The next unclaimed bank still takes NBG kinds. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::PatternNames,
                                           0x20000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::Ok));
}

/* RBG0's own image data: the inverse bank rule (p06_20, p06_40). */
static void test_rotation_image_bank_rules(void) {
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    /* Without a claim there is nowhere rotation names may live. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationNames,
                                           0x00000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::BankUnclaimed));
    ASSERT_EQ(static_cast<int>(plan::claim_banks(s, 0x03u)),   /* A0 + A1 */
              static_cast<int>(plan::Status::Ok));
    /* Rotation character windows obey the 32 KiB window alignment. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationChars,
                                           0x28000u, 0x8000u, 0x8000u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationChars,
                                           0x20001u, 0x40u, 2u)),
              static_cast<int>(plan::Status::BadAlign));
    /* Claiming more is fine; giving A1 back is refused while the window
     * is staged, and the refusal changes nothing. */
    ASSERT_EQ(static_cast<int>(plan::claim_banks(s, 0x03u)),
              static_cast<int>(plan::Status::Ok));   /* idempotent */
    ASSERT_EQ(static_cast<int>(plan::release_banks(s, 0x02u)),
              static_cast<int>(plan::Status::BanksBusy));
    ASSERT_EQ(s.claimed_banks, 0x03u);
    /* A claim changed under a staged NBG reservation: commit re-checks the
     * bank rules and flips nothing (the transaction stays open). */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationNames,
                                           0x00000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::commit(s)), static_cast<int>(plan::Status::Ok));
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::PatternNames,
                                           0x40000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::Ok));
    s.claimed_banks = 0x0Fu;   /* a claim the ledger did not see */
    ASSERT_EQ(static_cast<int>(plan::commit(s)),
              static_cast<int>(plan::Status::BankClaimed));
    ASSERT_EQ(s.last_conflict_base, 0x40000u);
    ASSERT_EQ(plan::usage(s).staged_count, 1u);   /* nothing flipped */
    s.claimed_banks = 0x03u;
    ASSERT_EQ(static_cast<int>(plan::commit(s)), static_cast<int>(plan::Status::Ok));
    /* Releasing a bank that hosts committed rotation image data is refused. */
    ASSERT_EQ(static_cast<int>(plan::release_banks(s, 0x01u)),
              static_cast<int>(plan::Status::BanksBusy));
    ASSERT_EQ(static_cast<int>(plan::release_banks(s, 0x04u)),
              static_cast<int>(plan::Status::Ok));   /* B0 never claimed... */
    ASSERT_EQ(s.claimed_banks, 0x03u);
}

/* RBG0's bitmap-format picture: claimed banks like the other image data, but
 * on bank edges and never next to RBG1. */
static void test_rotation_bitmap_rules(void) {
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    /* Nowhere to live without a claim. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationBitmap,
                                           0x40000u, 0x20000u, 0x20000u)),
              static_cast<int>(plan::Status::BankUnclaimed));
    ASSERT_EQ(static_cast<int>(plan::claim_banks(s, 0x04u)),   /* B0 */
              static_cast<int>(plan::Status::Ok));
    /* The base register counts in 0x20000 units. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationBitmap,
                                           0x50000u, 0x10000u, 0x20000u)),
              static_cast<int>(plan::Status::BadAlign));
    /* A picture that runs on into an unclaimed bank has no fetch there. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationBitmap,
                                           0x40000u, 0x40000u, 0x20000u)),
              static_cast<int>(plan::Status::BankUnclaimed));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationBitmap,
                                           0x40000u, 0x20000u, 0x20000u)),
              static_cast<int>(plan::Status::Ok));
    /* Every other reservation overlapping the picture is refused. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x41000u, 0x100u, 2u)),
              static_cast<int>(plan::Status::Overlap));
    ASSERT_EQ(s.last_conflict_base, 0x40000u);
    /* A claim taken away under the staged picture stops the commit. */
    s.claimed_banks = 0u;
    ASSERT_EQ(static_cast<int>(plan::commit(s)),
              static_cast<int>(plan::Status::BankUnclaimed));
    s.claimed_banks = 0x04u;
    ASSERT_EQ(static_cast<int>(plan::commit(s)), static_cast<int>(plan::Status::Ok));
    /* The live picture holds its bank and keeps RBG1 out of every bank. */
    ASSERT_EQ(static_cast<int>(plan::release_banks(s, 0x04u)),
              static_cast<int>(plan::Status::BanksBusy));
    ASSERT_EQ(static_cast<int>(plan::claim_rbg1_banks(s, 0x08u)),
              static_cast<int>(plan::Status::BanksBusy));
    ASSERT_EQ(s.rbg1_banks, 0u);
    /* Released, both go through; a picture then refuses the RBG1 reservation
     * even inside a bank that is also claimed. */
    ASSERT_EQ(static_cast<int>(plan::release(s, 0x40000u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::claim_rbg1_banks(s, 0x08u)),
              static_cast<int>(plan::Status::Ok));
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationBitmap,
                                           0x40000u, 0x20000u, 0x20000u)),
              static_cast<int>(plan::Status::BankClaimed));
    ASSERT_EQ(static_cast<int>(plan::release_rbg1_banks(s, 0x08u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationBitmap,
                                           0x40000u, 0x20000u, 0x20000u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::release_banks(s, 0x04u)),
              static_cast<int>(plan::Status::BanksBusy));
}

static void test_claim_conflicts_and_idempotence(void) {
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::PatternNames,
                                           0x20000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::commit(s)), static_cast<int>(plan::Status::Ok));
    /* A1 hosts a committed pattern-name table: claiming it is refused
     * and changes nothing. */
    ASSERT_EQ(static_cast<int>(plan::claim_banks(s, 0x02u)),
              static_cast<int>(plan::Status::BanksBusy));
    ASSERT_EQ(s.claimed_banks, 0u);
    ASSERT_EQ(static_cast<int>(plan::claim_banks(s, 0x01u)),
              static_cast<int>(plan::Status::Ok));
    /* Re-claiming a claimed bank is fine. */
    ASSERT_EQ(static_cast<int>(plan::claim_banks(s, 0x01u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::release_banks(s, 0x01u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::release_banks(s, 0x20u)),
              static_cast<int>(plan::Status::BadRange));
    ASSERT_EQ(static_cast<int>(plan::claim_banks(s, 0x00u)),
              static_cast<int>(plan::Status::BadRange));
}

static void test_rbg1_banks_let_params_live_in_b(void) {
    /* Manual p06_20: RDBSB0/B1 must be 00B (None) while RBG1 displays, and
     * the rotation parameter table may live in a None-role bank. The
     * planner keeps RotationParams/Coefficients out of RBG0-claimed banks
     * but lets them live in RBG1-reserved banks. */
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    /* B1 tail, away from the future RBG1 names region. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationParams,
                                           0x70000u, 0x60u, 4u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationParams,
                                           0x70200u, 0x60u, 4u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::commit(s)),
              static_cast<int>(plan::Status::Ok));
    /* The parameter tables in B1 do not block claim_rbg1_banks(B0+B1). */
    ASSERT_EQ(static_cast<int>(plan::claim_rbg1_banks(s, 0x04u | 0x08u)),
              static_cast<int>(plan::Status::Ok));
    /* RBG1 image data (RotationNames/RotationChars) can land in B0/B1
     * after the RBG1 bank claim. */
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationChars,
                                           0x40000u, 0x8000u, 0x8000u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationNames,
                                           0x60000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::commit(s)),
              static_cast<int>(plan::Status::Ok));
    /* NBG ranges cannot move into a reserved-for-RBG1 bank: the VDP2
     * routes B0/B1 names/chars to RBG1 only while RBG1 displays. */
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::PatternNames,
                                           0x48000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::BankClaimed));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Characters,
                                           0x68000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::BankClaimed));
    plan::rollback(s, [](void*, uint32_t) {}, nullptr);
}

static void test_rbg1_release_refused_while_image_data_lives(void) {
    plan::PlanState s;
    plan::init(s);
    ASSERT_EQ(static_cast<int>(plan::claim_rbg1_banks(s, 0x04u | 0x08u)),
              static_cast<int>(plan::Status::Ok));
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationChars,
                                           0x40000u, 0x8000u, 0x8000u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::commit(s)),
              static_cast<int>(plan::Status::Ok));
    /* release_rbg1_banks(B0) refused because RBG1 chars live there. */
    ASSERT_EQ(static_cast<int>(plan::release_rbg1_banks(s, 0x04u)),
              static_cast<int>(plan::Status::BanksBusy));
    /* release_rbg1_banks(B1) ok - no RBG1 image data in B1. */
    ASSERT_EQ(static_cast<int>(plan::release_rbg1_banks(s, 0x08u)),
              static_cast<int>(plan::Status::Ok));
    /* Now release the B0 reservation - refused again because chars live there. */
    ASSERT_EQ(static_cast<int>(plan::release_rbg1_banks(s, 0x04u)),
              static_cast<int>(plan::Status::BanksBusy));
}

static void test_claim_banks_and_rbg1_compose(void) {
    /* RBG0 image in A0, RBG1 image in B0+B1: the parameter region sits
     * in B0's tail, away from the RBG1 chars window. */
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationParams,
                                           0x50000u, 0x60u, 4u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::commit(s)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::claim_banks(s, 0x01u | 0x02u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::claim_rbg1_banks(s, 0x04u | 0x08u)),
              static_cast<int>(plan::Status::Ok));
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationNames,
                                           0x00000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationChars,
                                           0x20000u, 0x8000u, 0x8000u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationNames,
                                           0x60000u, 0x2000u, 2u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::RotationChars,
                                           0x40000u, 0x8000u, 0x8000u)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::commit(s)),
              static_cast<int>(plan::Status::Ok));
}

static void test_commit_and_rollback_ledger(void) {
    plan::PlanState s;
    int frees = 0;
    plan::init(s);
    plan::open(s);
    uint32_t a = 0u;
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Characters,
                                           0x10000u, 0x1000u, 2u)), 0);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Characters,
                                           0x20000u, 0x1000u, 2u)), 0);
    a = 0x10000u;
    plan::Usage u = plan::usage(s);
    ASSERT_EQ(u.staged_count, 2u);
    ASSERT_EQ(u.committed_count, 0u);
    ASSERT_EQ(static_cast<int>(plan::rollback(s,
            [](void* ctx, uint32_t) { (*static_cast<int*>(ctx))++; return; },
            &frees)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(frees, 2);
    ASSERT_EQ(plan::usage(s).staged_count, 0u);
    /* Nothing was committed: the ranges can be staged again. */
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Characters, a, 0x1000u, 2u)), 0);
    ASSERT_EQ(static_cast<int>(plan::commit(s)), static_cast<int>(plan::Status::Ok));
    u = plan::usage(s);
    ASSERT_EQ(u.committed_count, 1u);
    ASSERT_EQ(u.reserved_bytes, 0x1000u);
    /* Committed ranges are still overlap-checked against new stagings. */
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           a + 0x800u, 0x100u, 2u)),
              static_cast<int>(plan::Status::Overlap));
    /* Release frees the ledger entry (the byte owner is the API test's
     * business); releasing a staged base is refused. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x40000u, 0x100u, 2u)), 0);
    ASSERT_EQ(static_cast<int>(plan::release(s, 0x40000u)),
              static_cast<int>(plan::Status::NotFound));
    ASSERT_EQ(static_cast<int>(plan::rollback(s, nullptr, nullptr)), 0);
    ASSERT_EQ(static_cast<int>(plan::release(s, a)),
              static_cast<int>(plan::Status::Ok));
    ASSERT_EQ(static_cast<int>(plan::release(s, a)),
              static_cast<int>(plan::Status::NotFound));
}

static void test_ledger_capacity(void) {
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    for (uint8_t i = 0u; i < plan::kMaxReservations; ++i) {
        /* Every range disjoint: bank B1 has room for 24 tiny blocks. */
        const uint32_t base = 0x60000u + i * 0x100u;
        const int st = static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                                    base, 0x100u, 2u));
        ASSERT_EQ(st, 0);
    }
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x7F000u, 0x100u, 2u)),
              static_cast<int>(plan::Status::TableFull));
    ASSERT_EQ(plan::usage(s).staged_count,
              static_cast<uint32_t>(plan::kMaxReservations));
}

static void test_telemetry(void) {
    plan::PlanState s;
    plan::init(s);
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::PatternNames,
                                           0x00000u, 0x2000u, 2u)), 0);
    /* A range crossing the A0/A1 edge is attributed to both banks, but
     * only once committed: staged ranges are counted separately. */
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Characters,
                                           0x1F000u, 0x4000u, 2u)), 0);
    plan::Usage u = plan::usage(s);
    ASSERT_EQ(u.committed_count, 0u);
    ASSERT_EQ(u.staged_count, 2u);
    ASSERT_EQ(u.reserved_bytes, 0u);
    ASSERT_EQ(u.reserved_bytes_per_bank[0], 0u);
    ASSERT_EQ(static_cast<int>(plan::commit(s)), 0);
    u = plan::usage(s);
    ASSERT_EQ(u.committed_count, 2u);
    ASSERT_EQ(u.staged_count, 0u);
    ASSERT_EQ(u.reserved_bytes, 0x6000u);
    ASSERT_EQ(u.reserved_bytes_per_bank[0], 0x2000u + 0x4000u);
    ASSERT_EQ(u.reserved_bytes_per_bank[1], 0x4000u);
    ASSERT_EQ(u.reserved_bytes_per_bank[2], 0u);
    ASSERT_EQ(u.reserved_bytes_per_bank[3], 0u);
    /* Refusals and the last reason are reported. */
    plan::open(s);
    ASSERT_EQ(static_cast<int>(plan::stage(s, plan::Kind::Scratch,
                                           0x000100u, 0x10u, 2u)),
              static_cast<int>(plan::Status::Overlap));
    u = plan::usage(s);
    ASSERT_EQ(u.refusals, 1u);
    ASSERT_EQ(u.last_status, static_cast<uint32_t>(plan::Status::Overlap));
    ASSERT_EQ(u.last_conflict_base, 0x00000u);
    ASSERT_EQ(static_cast<int>(plan::rollback(s, nullptr, nullptr)), 0);
    u = plan::usage(s);
    ASSERT_EQ(u.staged_count, 0u);
    ASSERT_EQ(u.last_status, static_cast<uint32_t>(plan::Status::Overlap));
}

int main(void) {
    test_transaction_lifecycle();
    test_kind_minimum_alignments();
    test_range_rules();
    test_overlap_detection();
    test_bank_roles();
    test_rotation_image_bank_rules();
    test_rotation_bitmap_rules();
    test_claim_conflicts_and_idempotence();
    test_rbg1_banks_let_params_live_in_b();
    test_rbg1_release_refused_while_image_data_lives();
    test_claim_banks_and_rbg1_compose();
    test_commit_and_rollback_ledger();
    test_ledger_capacity();
    test_telemetry();
    printf("test_vdp2_plan_logic: all tests passed\n");
    return 0;
}
