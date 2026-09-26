#ifndef SATURN_SMPC_H
#define SATURN_SMPC_H

#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The SMPC's own services: real-time clock, cartridge and area code, and the
 * four battery-backed bytes. Controller data is in saturn/input.h.
 *
 * The SEGA SMPC manual marks MSHON, CDON, CDOFF, SYSRES and CKCHG352/320 as
 * prohibited for applications (the system libraries own them), so they are
 * not exposed; the system reset button is controlled by sat_smpc_reset_enable. */

typedef struct sat_rtc_time {
    uint16_t year;     /* 1980..2099 */
    uint8_t month;     /* 1..12 */
    uint8_t day;       /* 1..31 */
    uint8_t weekday;   /* 0 = Sunday; set by sat_rtc_set from the date */
    uint8_t hour;      /* 0..23 */
    uint8_t minute;    /* 0..59 */
    uint8_t second;    /* 0..59 */
} sat_rtc_time_t;

typedef struct sat_smpc_status {
    uint8_t rtc_set;          /* 1 once the clock was set since the SMPC's cold reset */
    uint8_t reset_disabled;   /* 1 when the reset button is ignored */
    uint8_t cartridge_code;   /* CTG1-0 */
    uint8_t area_code;        /* 1 Japan, 4 North America, 0xC Europe ... (SMPC manual) */
    uint8_t system_status1;   /* raw OREG10 */
    uint8_t system_status2;   /* raw OREG11 */
    uint8_t smem[4];          /* battery-backed bytes */
    sat_rtc_time_t time;
} sat_smpc_status_t;

/* Reads the SMPC status block (INTBACK without peripheral data).
 * SAT_ERR_IO when the SMPC never answers or returns a clock that is not BCD. */
sat_result_t sat_smpc_status(sat_smpc_status_t* out_status);

sat_result_t sat_rtc_get(sat_rtc_time_t* out_time);

/* SETTIME. The weekday field is ignored and computed from the date. Values
 * the SMPC would leave undefined (a day past the month's end, hour 24 ...)
 * are refused with SAT_ERR_INVALID_ARG before anything is written. */
sat_result_t sat_rtc_set(const sat_rtc_time_t* time);

/* Writes the four battery-backed bytes (SETSMEM) / reads them back. */
sat_result_t sat_smem_set(const uint8_t smem[4]);
sat_result_t sat_smem_get(uint8_t out_smem[4]);

/* RESENAB / RESDISA: lets or blocks the reset button. */
sat_result_t sat_smpc_reset_enable(int enabled);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SMPC_H */
