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

#ifdef __cplusplus
}
#endif
#endif
