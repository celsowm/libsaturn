#ifndef SATURN_CORE_FMT_LOGIC_HPP
#define SATURN_CORE_FMT_LOGIC_HPP

/* Pure, host-testable decimal formatting. No hardware access, so
 * tests/host/test_fmt_logic.cpp links this directly. */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/fmt.h"

namespace saturn::core::fmt {

/* Digits are produced least-significant first, so they are collected in a
 * scratch buffer and reversed rather than requiring a division pass to count
 * them first. 10 digits covers the whole uint32 range. */
inline uint16_t digits_u32(uint32_t value, char* scratch) {
    uint16_t n = 0;
    if (value == 0u) {
        scratch[n++] = '0';
        return n;
    }
    while (value > 0u) {
        scratch[n++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    }
    return n;
}

inline sat_result_t write_u32(uint32_t value, char* out, uint16_t out_size, uint16_t* out_len) {
    if (out == nullptr || out_size == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    char scratch[10];
    const uint16_t n = digits_u32(value, scratch);
    if (static_cast<uint32_t>(n) + 1u > out_size) {
        return SAT_ERR_CAPACITY;
    }
    for (uint16_t i = 0; i < n; ++i) {
        out[i] = scratch[n - 1u - i];
    }
    out[n] = '\0';
    if (out_len != nullptr) {
        *out_len = n;
    }
    return SAT_OK;
}

inline sat_result_t write_i32(int32_t value, char* out, uint16_t out_size, uint16_t* out_len) {
    if (out == nullptr || out_size == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (value >= 0) {
        return write_u32(static_cast<uint32_t>(value), out, out_size, out_len);
    }
    /* Negating INT32_MIN overflows, so widen before taking the magnitude. */
    const uint32_t magnitude =
        static_cast<uint32_t>(-static_cast<int64_t>(value));
    if (out_size < 2u) {
        return SAT_ERR_CAPACITY;
    }
    uint16_t len = 0;
    const sat_result_t st =
        write_u32(magnitude, out + 1, static_cast<uint16_t>(out_size - 1u), &len);
    if (st != SAT_OK) {
        return st;
    }
    out[0] = '-';
    if (out_len != nullptr) {
        *out_len = static_cast<uint16_t>(len + 1u);
    }
    return SAT_OK;
}

inline sat_result_t write_u32_padded(
    uint32_t value,
    uint8_t digits,
    char* out,
    uint16_t out_size,
    uint16_t* out_len
) {
    if (out == nullptr || out_size == 0u || digits == 0u || digits > 10u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (static_cast<uint32_t>(digits) + 1u > out_size) {
        return SAT_ERR_CAPACITY;
    }
    /* Emit from the least significant digit backwards, so a value too large
     * for `digits` loses its high end and the field keeps its fixed width. */
    for (int i = static_cast<int>(digits) - 1; i >= 0; --i) {
        out[i] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    }
    out[digits] = '\0';
    if (out_len != nullptr) {
        *out_len = digits;
    }
    return SAT_OK;
}

inline sat_result_t write_label_u32(
    const char* label,
    uint32_t value,
    char* out,
    uint16_t out_size,
    uint16_t* out_len
) {
    if (label == nullptr || out == nullptr || out_size == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    uint16_t n = 0;
    while (label[n] != '\0') {
        if (static_cast<uint32_t>(n) + 1u >= out_size) {
            return SAT_ERR_CAPACITY;
        }
        out[n] = label[n];
        ++n;
    }
    uint16_t len = 0;
    const sat_result_t st =
        write_u32(value, out + n, static_cast<uint16_t>(out_size - n), &len);
    if (st != SAT_OK) {
        return st;
    }
    if (out_len != nullptr) {
        *out_len = static_cast<uint16_t>(n + len);
    }
    return SAT_OK;
}

}  // namespace saturn::core::fmt

#endif /* SATURN_CORE_FMT_LOGIC_HPP */
