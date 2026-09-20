#ifndef SATURN_FMT_H
#define SATURN_FMT_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Minimal number formatting                                           */
/* ------------------------------------------------------------------ */
/* There is no printf in a freestanding Saturn build, and pulling one in costs
 * more code space than the whole HUD it would serve. These write decimal text
 * into a caller-supplied buffer instead.
 *
 * Every function NUL-terminates on success and writes nothing on error.
 * `out_len` may be NULL when the length is not needed.
 */

/* Longest output of sat_fmt_u32 ("4294967295") plus the terminator. */
#define SAT_FMT_U32_MAX 11u

/* Longest output of sat_fmt_i32 ("-2147483648") plus the terminator. */
#define SAT_FMT_I32_MAX 12u

sat_result_t sat_fmt_u32(uint32_t value, char* out, uint16_t out_size, uint16_t* out_len);
sat_result_t sat_fmt_i32(int32_t value, char* out, uint16_t out_size, uint16_t* out_len);

/* Zero-padded to `digits` (truncating the high end if the value does not
 * fit), e.g. digits = 6 turns 1250 into "001250". A scoreboard that keeps a
 * fixed width does not jitter as the score grows. */
sat_result_t sat_fmt_u32_padded(
    uint32_t value,
    uint8_t digits,
    char* out,
    uint16_t out_size,
    uint16_t* out_len
);

/* Writes `label` followed by the decimal value, e.g. "SCORE " + 1250 ->
 * "SCORE 1250". This is the shape essentially every HUD needs, and doing it
 * by hand is where off-by-one buffer bugs come from. */
sat_result_t sat_fmt_label_u32(
    const char* label,
    uint32_t value,
    char* out,
    uint16_t out_size,
    uint16_t* out_len
);

/* Longest output of sat_fmt_fx16 ("-32768.0000") plus the terminator. */
#define SAT_FMT_FX16_MAX 12u

/* Signed 16.16 fixed point with `decimals` (0..4) fractional digits, e.g.
 * -0.3 -> "-0.3". The fraction is TRUNCATED, not rounded, so the printed
 * digits never disagree with the integer part they follow.
 *
 * The sign is written whenever the value is negative, including when the
 * integer part is zero: a falling object at -0.3 must not read as 0.3.
 * Reaching for sat_fx16_to_int instead silently discards the fraction, and
 * assembling the two halves by hand is exactly the buffer arithmetic this
 * module exists to remove. */
sat_result_t sat_fmt_fx16(
    sat_fx16_t value,
    uint8_t decimals,
    char* out,
    uint16_t out_size,
    uint16_t* out_len
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_FMT_H */
