#include "saturn/fmt.h"

#include "src/core/runtime/format_logic.hpp"

using namespace saturn::core::fmt;

extern "C" sat_result_t sat_fmt_u32(uint32_t value, char* out, uint16_t out_size, uint16_t* out_len) {
    return write_u32(value, out, out_size, out_len);
}

extern "C" sat_result_t sat_fmt_i32(int32_t value, char* out, uint16_t out_size, uint16_t* out_len) {
    return write_i32(value, out, out_size, out_len);
}

extern "C" sat_result_t sat_fmt_u32_padded(
    uint32_t value,
    uint8_t digits,
    char* out,
    uint16_t out_size,
    uint16_t* out_len
) {
    return write_u32_padded(value, digits, out, out_size, out_len);
}

extern "C" sat_result_t sat_fmt_label_u32(
    const char* label,
    uint32_t value,
    char* out,
    uint16_t out_size,
    uint16_t* out_len
) {
    return write_label_u32(label, value, out, out_size, out_len);
}

extern "C" sat_result_t sat_fmt_fx16(
    sat_fx16_t value,
    uint8_t decimals,
    char* out,
    uint16_t out_size,
    uint16_t* out_len
) {
    return write_fx16(value, decimals, out, out_size, out_len);
}
