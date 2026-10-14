#ifndef LIBSATURN_BOOT_TRACE_H
#define LIBSATURN_BOOT_TRACE_H

#include <stdint.h>

/* Opt-in application-owned, initialized data. No BIOS/mailbox address is
 * reserved. The weak symbol resolves to zero in applications without a trace.
 * ST-040: the AIP executes after first-read completion. */
typedef struct sat_boot_trace {
    uint32_t magic, version, bytes, stage;
    uint32_t completed, reached, result, entry_sr, entry_vbr, entry_sp;
} sat_boot_trace_t;

enum {
    SAT_BOOT_AIP=1, SAT_BOOT_CRT_ENTRY=2, SAT_BOOT_CRT_READY=3,
    SAT_BOOT_MAIN=4, SAT_BOOT_INIT_BEGIN=5, SAT_BOOT_EARLY_BEGIN=6,
    SAT_BOOT_EARLY_END=7, SAT_BOOT_INIT_END=8, SAT_BOOT_FIRST_FRAME=9,
    SAT_BOOT_CD=10, SAT_BOOT_ISO=11, SAT_BOOT_SECTOR=12, SAT_BOOT_STREAM=13
};

#ifdef __cplusplus
extern "C" {
#endif
extern volatile sat_boot_trace_t sat_boot_trace_record __attribute__((weak));
#ifdef __cplusplus
}
#endif

static inline void sat_boot_mark(uint32_t stage, int completed, int32_t result) {
    uintptr_t address=(uintptr_t)&sat_boot_trace_record;
    if (!address) return;
#if defined(__sh__) || defined(__SH2__)
    address |= 0x20000000u;
#endif
    volatile sat_boot_trace_t* record=(volatile sat_boot_trace_t*)address;
    record->reached |= 1u << stage;
    record->stage=stage;
    record->result=(uint32_t)result;
    if (completed) record->completed=stage;
}
#endif
