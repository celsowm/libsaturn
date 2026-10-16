/* Transactional VDP2 resource plan: the C API in include/saturn/vdp2_alloc.h
 * over the pure policy in resource_plan_logic.hpp.
 *
 * Ownership stays where it was: the byte free list (allocator.cpp) owns
 * every VRAM byte exclusively, the layer manager owns the cycle pattern
 * registers, and this plan owns the semantic ledger — which range is
 * which kind, which banks are whole-claimed for rotation, and the
 * all-or-nothing transaction. Placement failures never leave bytes
 * behind: a reservation is placed in the free list, recorded in the
 * ledger, and undone in the free list when the ledger refuses it.
 *
 * Single-threaded like the free list (examples run on the SH-2 master;
 * the dual_sh2 contract excludes the slave from VDP2 ownership). */

#include <stdint.h>

#include "saturn/vdp2_alloc.h"

#include "resource_plan_logic.hpp"
#include "vram_internal.hpp"

namespace {

namespace plan = saturn::hal::vdp2::plan;

plan::PlanState g_plan;
sat_result_t g_last_sat_status = SAT_OK;
uint8_t g_plan_inited = 0u;

void plan_ensure_init() {
    if (g_plan_inited == 0u) {
        plan::init(g_plan);
        g_last_sat_status = SAT_OK;
        g_plan_inited = 1u;
    }
}

sat_result_t map_status(plan::Status status) {
    switch (status) {
        case plan::Status::Ok:         return SAT_OK;
        case plan::Status::NotOpen:    return SAT_ERR_NOT_INITIALIZED;
        case plan::Status::AlreadyOpen: return SAT_ERR_BUSY;
        case plan::Status::TableFull:
        case plan::Status::Exhausted:
        case plan::Status::Overlap:    return SAT_ERR_CAPACITY;
        case plan::Status::BadKind:
        case plan::Status::BadAlign:
        case plan::Status::BadRange:   return SAT_ERR_INVALID_ARG;
        case plan::Status::BankClaimed:
        case plan::Status::BankUnclaimed:
        case plan::Status::BanksBusy:  return SAT_ERR_BUSY;
        case plan::Status::NotFound:   return SAT_ERR_NOT_FOUND;
    }
    return SAT_ERR_INVALID_ARG;
}

/* Records the sat_result_t shadow of the policy status. */
sat_result_t finish(sat_result_t result) {
    if (result != SAT_OK) {
        g_last_sat_status = result;
    }
    return result;
}

/* Frees one range in the byte owner (rollback callback). */
void free_range_thunk(void* /*ctx*/, uint32_t base) {
    (void)sat_vdp2_vram_free(base);
}

/* Placement for one reservation: first fit over the banks the kind may
 * use. `alignment_words` is the byte alignment divided by two (the free
 * list aligns in 2-byte words). A kind whose reads are timed by the
 * cycle pattern registers (or claimed whole for rotation) is placed
 * inside one maximal run of unclaimed banks: it may span bank edges
 * inside the run (a 0x40000-byte bitmap crosses A0/A1; the layer manager
 * times both banks), but never reaches into a bank claimed for rotation.
 * The inverse holds for RBG0's own image data (names, character windows):
 * the rotation data bank bits fetch it, so it is placed inside one
 * maximal run of CLAIMED banks and refused while none exists. Kinds that
 * need no cycle pattern use the whole space. Returns 1 with the base when
 * the free list placed the range. */
int place_kind(plan::Kind kind, uint32_t bytes, uint32_t alignment_words, uint32_t* out_base) {
    const bool unclaimed_run = plan::kind_needs_unclaimed_bank(kind);
    const bool claimed_run = plan::kind_requires_claimed_bank(kind);
    if (unclaimed_run || claimed_run) {
        uint32_t run_start = 0u;
        for (uint8_t b = 0u; b < plan::kBankCount; ++b) {
            const bool break_here = claimed_run
                ? (plan::kBankMask[b] & g_plan.claimed_banks) == 0u
                : (plan::kBankMask[b] & g_plan.claimed_banks) != 0u;
            if (break_here) {
                /* Close the run at the breaking bank's lower edge. */
                if (vram_alloc_in(run_start, plan::kBankBase[b],
                                  bytes, alignment_words, out_base)) {
                    return 1;
                }
                run_start = plan::kBankBase[b] + plan::kBankBytes;
            }
        }
        /* The final run reaches the end of VRAM (empty when the last bank
         * breaks it: start >= end refuses the request). */
        return vram_alloc_in(run_start, plan::kVramBytes,
                             bytes, alignment_words, out_base);
    }
    return vram_alloc_in(0u, plan::kVramBytes, bytes, alignment_words, out_base);
}

}  /* namespace */

extern "C" {

sat_result_t sat_vdp2_plan_begin(void) {
    plan_ensure_init();
    return finish(map_status(plan::open(g_plan)));
}

sat_result_t sat_vdp2_plan_reserve(sat_vdp2_reserve_kind_t kind, uint32_t bytes,
                                   uint32_t alignment_bytes, uint32_t* out_base) {
    plan_ensure_init();
    if (out_base == nullptr) {
        return finish(SAT_ERR_INVALID_ARG);
    }
    if (!plan::kind_valid(static_cast<plan::Kind>(kind))) {
        (void)plan::note_refusal(g_plan, plan::Status::BadKind);
        return finish(SAT_ERR_INVALID_ARG);
    }
    const plan::Kind k = static_cast<plan::Kind>(kind);
    if (bytes == 0u) {
        (void)plan::note_refusal(g_plan, plan::Status::BadRange);
        return finish(SAT_ERR_INVALID_ARG);
    }
    const uint32_t want = alignment_bytes ? alignment_bytes
                                           : plan::kind_min_align_bytes(k);
    if ((want & 1u) != 0u || want < plan::kind_min_align_bytes(k)) {
        (void)plan::note_refusal(g_plan, plan::Status::BadAlign);
        return finish(SAT_ERR_INVALID_ARG);
    }
    uint32_t base = 0u;
    if (!place_kind(k, bytes, want / 2u, &base)) {
        /* For RBG0 image data the honest reason is that no claimed run can
         * host the range, never generic exhaustion. */
        const plan::Status why = plan::kind_requires_claimed_bank(k)
            ? plan::Status::BankUnclaimed
            : plan::Status::Exhausted;
        (void)plan::note_refusal(g_plan, why);
        return finish(map_status(why));
    }
    const plan::Status staged = plan::stage(g_plan, k, base, bytes, want);
    if (staged != plan::Status::Ok) {
        /* The ledger refused (full, or a rule the placement could not see):
         * undo the placement so nothing is lost behind the refusal. */
        (void)sat_vdp2_vram_free(base);
        return finish(map_status(staged));
    }
    *out_base = base;
    return SAT_OK;
}

sat_result_t sat_vdp2_plan_reserve_at(sat_vdp2_reserve_kind_t kind, uint32_t base,
                                      uint32_t bytes) {
    plan_ensure_init();
    if (!plan::kind_valid(static_cast<plan::Kind>(kind))) {
        (void)plan::note_refusal(g_plan, plan::Status::BadKind);
        return finish(SAT_ERR_INVALID_ARG);
    }
    const plan::Kind k = static_cast<plan::Kind>(kind);
    /* Semantic validation first (alignment, bounds, bank rules, overlap);
     * only a range the ledger accepts reaches the byte owner. */
    const plan::Status staged = plan::stage(g_plan, k, base, bytes,
                                            plan::kind_min_align_bytes(k));
    if (staged != plan::Status::Ok) {
        return finish(map_status(staged));
    }
    if (!vram_alloc_at(base, bytes)) {
        (void)plan::unstage(g_plan, base);
        (void)plan::note_refusal(g_plan, plan::Status::Exhausted);
        return finish(SAT_ERR_CAPACITY);
    }
    return SAT_OK;
}

sat_result_t sat_vdp2_plan_commit(void) {
    plan_ensure_init();
    return finish(map_status(plan::commit(g_plan)));
}

sat_result_t sat_vdp2_plan_rollback(void) {
    plan_ensure_init();
    return finish(map_status(plan::rollback(g_plan, free_range_thunk, nullptr)));
}

sat_result_t sat_vdp2_plan_release(uint32_t base) {
    plan_ensure_init();
    const plan::Status released = plan::release(g_plan, base);
    if (released != plan::Status::Ok) {
        return finish(map_status(released));
    }
    return finish(sat_vdp2_vram_free(base));
}

sat_result_t sat_vdp2_plan_release_all(void) {
    plan_ensure_init();
    for (;;) {
        /* Release the first committed reservation until none is left. */
        uint32_t base = 0u;
        uint8_t found = 0u;
        for (uint8_t i = 0u; i < plan::kMaxReservations; ++i) {
            if (static_cast<plan::SlotState>(g_plan.slots[i].state)
                == plan::SlotState::Committed) {
                base = g_plan.slots[i].base;
                found = 1u;
                break;
            }
        }
        if (found == 0u) break;
        const plan::Status released = plan::release(g_plan, base);
        if (released != plan::Status::Ok) {
            return finish(map_status(released));
        }
        const sat_result_t freed = sat_vdp2_vram_free(base);
        if (freed != SAT_OK) {
            return finish(freed);
        }
    }
    return SAT_OK;
}

sat_result_t sat_vdp2_plan_claim_banks(uint8_t bank_mask) {
    plan_ensure_init();
    return finish(map_status(plan::claim_banks(g_plan, bank_mask)));
}

sat_result_t sat_vdp2_plan_release_banks(uint8_t bank_mask) {
    plan_ensure_init();
    return finish(map_status(plan::release_banks(g_plan, bank_mask)));
}

void sat_vdp2_plan_reset(void) {
    plan::init(g_plan);
    g_last_sat_status = SAT_OK;
    g_plan_inited = 1u;
}

void sat_vdp2_plan_usage(sat_vdp2_plan_usage_t* out) {
    plan_ensure_init();
    if (out == nullptr) return;
    const plan::Usage u = plan::usage(g_plan);
    out->committed_count = u.committed_count;
    out->staged_count = u.staged_count;
    out->reserved_bytes = u.reserved_bytes;
    for (uint8_t b = 0u; b < 4u; ++b) {
        out->reserved_bytes_per_bank[b] = u.reserved_bytes_per_bank[b];
        out->free_bytes_per_bank[b] =
            vram_free_bytes(plan::kBankBase[b], plan::kBankBase[b] + plan::kBankBytes);
    }
    out->claimed_banks = u.claimed_banks;
    out->refusals = u.refusals;
    out->last_status = g_last_sat_status;
    out->last_conflict_base = u.last_conflict_base;
}

}  /* extern "C" */
