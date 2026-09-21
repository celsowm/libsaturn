#ifndef SATURN_RESOURCE_PLAN_H
#define SATURN_RESOURCE_PLAN_H

#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A bounded, caller-owned content budget. It plans quantities; it never
 * allocates, repacks, or silently evicts an already referenced resource. */
typedef enum sat_resource_kind {
    SAT_RESOURCE_MAIN_RAM = 0,
    SAT_RESOURCE_WRAM = 1,
    SAT_RESOURCE_VDP1_COMMANDS = 2,
    SAT_RESOURCE_VDP1_VRAM = 3,
    SAT_RESOURCE_VDP2_VRAM = 4,
    SAT_RESOURCE_CRAM = 5,
    SAT_RESOURCE_AUDIO_STAGING = 6,
    SAT_RESOURCE_KIND_COUNT = 7
} sat_resource_kind_t;

typedef struct sat_resource_plan_entry {
    sat_resource_kind_t kind;
    uint8_t required;
    uint16_t alignment;
    uint32_t offset;
    uint32_t bytes;
} sat_resource_plan_entry_t;

typedef struct sat_resource_plan {
    sat_resource_plan_entry_t* entries;
    uint16_t count;
    uint16_t capacity;
    uint32_t used[SAT_RESOURCE_KIND_COUNT];
    uint32_t limits[SAT_RESOURCE_KIND_COUNT];
    uint8_t finalized;
} sat_resource_plan_t;

sat_result_t sat_resource_plan_requirements(
    uint16_t entry_capacity, uint32_t* out_bytes);
sat_result_t sat_resource_plan_init(
    sat_resource_plan_t* plan,
    sat_resource_plan_entry_t* storage,
    uint16_t capacity);
sat_result_t sat_resource_plan_set_limit(
    sat_resource_plan_t* plan, sat_resource_kind_t kind, uint32_t bytes);
sat_result_t sat_resource_plan_add(
    sat_resource_plan_t* plan, sat_resource_kind_t kind, uint32_t bytes,
    uint16_t alignment, uint8_t required);
sat_result_t sat_resource_plan_finalize(sat_resource_plan_t* plan);
sat_result_t sat_resource_plan_usage(
    const sat_resource_plan_t* plan, sat_resource_kind_t kind,
    uint32_t* out_bytes, uint32_t* out_limit);

#ifdef __cplusplus
}
#endif
#endif
