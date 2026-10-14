#ifndef OPTICAL_DIAGNOSTICS_H
#define OPTICAL_DIAGNOSTICS_H
#include "saturn/core.h"
#include "saturn/cd_block.h"
enum { OET_STEP_STATUS, OET_STEP_ISO, OET_STEP_READ1, OET_STEP_READ2,
       OET_STEP_READ32, OET_STEP_STREAM, OET_STEP_COUNT };
enum { OET_COMMAND_TIMEOUT_MS=2000, OET_OPERATION_TIMEOUT_MS=10000,
       OET_SUSTAINED_DURATION_MS=30000, OET_FIXTURE_SECTORS=32 };
typedef struct optical_results {
    uint32_t magic, version, frames, profile, build_id;
    uint32_t selected, current, completed, result, passes, failures;
    uint32_t lba, requested, transferred, crc, elapsed_ms, timeouts;
    uint32_t hirq, cr1, cr2, cr3, cr4, wait_mask, response_valid;
    uint32_t sustained_bytes, sustained_ms, cancelled;
} optical_results_t;
extern volatile optical_results_t g_optical_results;
sat_result_t optical_run_step(unsigned step);
const char* optical_step_name(unsigned step);
#endif
