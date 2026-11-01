#ifndef SATURN_SH2_H
#define SATURN_SH2_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { SAT_SH2_CCR_CACHE_ENABLE = 1u << 0 };

/* Read the calling SH-2's raw cache control register (SH7604 section 8.2).
 * Does not enable/disable/purge the cache and does not require sat_init().
 * On the Master it describes the Master; inside a verified Slave job it
 * describes the Slave. A harness's cache-emulation switch is a separate fact. */
uint8_t sat_sh2_cache_control(void);
/* The cache control register as start-up found it, before the library set the
 * full 4 KiB cache: 0 for the Master, 1 for the Slave once it has started.
 * Shows what the BIOS or a loader left. */
uint8_t sat_sh2_cache_control_at_start(uint8_t cpu);

#ifdef __cplusplus
}
#endif
#endif
