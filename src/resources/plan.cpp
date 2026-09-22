#include "saturn/resource_plan.h"

namespace {
static bool valid_kind(sat_resource_kind_t kind) {
    return kind >= SAT_RESOURCE_MAIN_RAM && kind < SAT_RESOURCE_KIND_COUNT;
}

static bool valid_alignment(uint16_t alignment) {
    return alignment != 0u && (alignment & (alignment - 1u)) == 0u;
}
}

extern "C" sat_result_t sat_resource_plan_requirements(
    uint16_t entry_capacity, uint32_t* out_bytes) {
    if (!out_bytes) return SAT_ERR_INVALID_ARG;
    *out_bytes = static_cast<uint32_t>(entry_capacity) *
                 static_cast<uint32_t>(sizeof(sat_resource_plan_entry_t));
    return SAT_OK;
}

extern "C" sat_result_t sat_resource_plan_init(
    sat_resource_plan_t* plan, sat_resource_plan_entry_t* storage,
    uint16_t capacity) {
    if (!plan || (!storage && capacity)) return SAT_ERR_INVALID_ARG;
    *plan = {};
    plan->entries = storage;
    plan->capacity = capacity;
    return SAT_OK;
}

extern "C" sat_result_t sat_resource_plan_set_limit(
    sat_resource_plan_t* plan, sat_resource_kind_t kind, uint32_t bytes) {
    if (!plan || !valid_kind(kind) || plan->finalized) return SAT_ERR_INVALID_ARG;
    plan->limits[kind] = bytes;
    return SAT_OK;
}

extern "C" sat_result_t sat_resource_plan_add(
    sat_resource_plan_t* plan, sat_resource_kind_t kind, uint32_t bytes,
    uint16_t alignment, uint8_t required) {
    if (!plan || !valid_kind(kind) || plan->finalized ||
        !valid_alignment(alignment) || (required > 1u))
        return SAT_ERR_INVALID_ARG;
    if (plan->count >= plan->capacity) return SAT_ERR_CAPACITY;
    const uint32_t current = plan->used[kind];
    const uint32_t mask = static_cast<uint32_t>(alignment) - 1u;
    if (current > UINT32_MAX - mask) return SAT_ERR_CAPACITY;
    const uint32_t offset = (current + mask) & ~mask;
    if (bytes > UINT32_MAX - offset) return SAT_ERR_CAPACITY;
    const uint32_t next = offset + bytes;
    if (plan->limits[kind] && next > plan->limits[kind])
        return required ? SAT_ERR_CAPACITY : SAT_ERR_BUSY;
    sat_resource_plan_entry_t& entry = plan->entries[plan->count];
    entry.kind = kind;
    entry.required = required;
    entry.alignment = alignment;
    entry.offset = offset;
    entry.bytes = bytes;
    plan->used[kind] = next;
    ++plan->count;
    return SAT_OK;
}

extern "C" sat_result_t sat_resource_plan_finalize(sat_resource_plan_t* plan) {
    if (!plan || (!plan->entries && plan->capacity)) return SAT_ERR_INVALID_ARG;
    plan->finalized = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_resource_plan_usage(
    const sat_resource_plan_t* plan, sat_resource_kind_t kind,
    uint32_t* out_bytes, uint32_t* out_limit) {
    if (!plan || !valid_kind(kind) || !out_bytes || !out_limit)
        return SAT_ERR_INVALID_ARG;
    *out_bytes = plan->used[kind];
    *out_limit = plan->limits[kind];
    return SAT_OK;
}
