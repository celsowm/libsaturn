#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/hal/vdp2/rbg0_logic.hpp"

namespace rbg0 = saturn::hal::vdp2::rbg0;

static void bitmap_spans_are_mapped_to_all_banks() {
    assert(rbg0::bitmap_bank_mask(0x00000u, 0x10000u) == 0x01u);
    assert(rbg0::bitmap_bank_mask(0x00000u, 0x20000u) == 0x03u);
    assert(rbg0::bitmap_bank_mask(0x10000u, 0x20000u) == 0x06u);
}

static void ramctl_replaces_fields_and_preserves_unrelated_bits() {
    const uint16_t previous = 0xB6A5u;
    const uint16_t value = rbg0::compose_ramctl(
        previous, true, 0x01u, 0xFFu, 0x00000u, 0x10000u, 0x10000u);

    assert(rbg0::bank_field(value, 0u) == 3u);
    assert(rbg0::bank_field(value, 1u) == 1u);
    assert(rbg0::bank_field(value, 2u) == rbg0::bank_field(previous, 2u));
    assert(rbg0::bank_field(value, 3u) == rbg0::bank_field(previous, 3u));
    assert((value & 0xFF00u) == ((previous | 0x1100u) & 0xFF00u));
}

static void ramctl_removes_previous_rbg0_fields() {
    const uint16_t previous = 0xB6A5u;
    const uint16_t value = rbg0::compose_ramctl(
        previous, true, 0x01u, 1u, 0x10000u, 0x10000u, 0x30000u);

    assert(rbg0::bank_field(value, 0u) == 0u);
    assert(rbg0::bank_field(value, 1u) == 3u);
    assert(rbg0::bank_field(value, 2u) == rbg0::bank_field(previous, 2u));
    assert(rbg0::bank_field(value, 3u) == 1u);
}

static void cycle_plan_marks_bitmap_and_parameter_banks() {
    const uint16_t low[4] = {0xAAAAu, 0xBBBBu, 0xCCCCu, 0xDDDDu};
    const uint16_t high[4] = {0x1111u, 0x2222u, 0x3333u, 0x4444u};

    const rbg0::CyclePlan plan = rbg0::make_cycle_plan(
        low, high, 0x10000u, 0x20000u, 0x30000u);

    assert(plan.changed[0] == false);
    assert(plan.changed[1] && plan.low[1] == 0x9E9Eu && plan.high[1] == 0x9E9Eu);
    assert(plan.changed[2] && plan.low[2] == 0x9E9Eu && plan.high[2] == 0x9E9Eu);
    assert(plan.changed[3] && plan.low[3] == 0x8E8Eu && plan.high[3] == 0x8E8Eu);
    assert(plan.low[0] == low[0] && plan.high[0] == high[0]);
}

int main() {
    bitmap_spans_are_mapped_to_all_banks();
    ramctl_replaces_fields_and_preserves_unrelated_bits();
    ramctl_removes_previous_rbg0_fields();
    cycle_plan_marks_bitmap_and_parameter_banks();
    std::puts("vdp2 RBG0 logic: OK");
    return 0;
}
