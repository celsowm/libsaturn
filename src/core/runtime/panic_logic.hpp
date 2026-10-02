#ifndef SATURN_CORE_PANIC_LOGIC_HPP
#define SATURN_CORE_PANIC_LOGIC_HPP

/* Pure, host-testable pieces of the fatal-error screen: result names, file
 * basenames, the report text and a 3x5 bitmap font that turns text into
 * horizontal rectangle runs. No hardware access, so
 * tests/host/test_panic_logic.cpp links this directly.
 *
 * The screen is drawn from rectangles on purpose. A panic can fire before any
 * font texture exists, with VDP2 in an unknown configuration, or in an example
 * that never touched text; rectangles are the one primitive that needs only an
 * initialised VDP1. */

#include <stdint.h>

#include "saturn/core.h"

namespace saturn::core::panic {

constexpr uint8_t kLineChars = 38u;   /* 38 cells x 8 px = 304 px at scale 2 */
constexpr uint8_t kLineCount = 4u;

inline const char* result_name(sat_result_t st) {
    switch (st) {
    case SAT_OK: return "OK";
    case SAT_ERR_INVALID_ARG: return "INVALID ARG";
    case SAT_ERR_NOT_INITIALIZED: return "NOT INITIALIZED";
    case SAT_ERR_CAPACITY: return "CAPACITY";
    case SAT_ERR_UNSUPPORTED: return "UNSUPPORTED";
    case SAT_ERR_IO: return "IO";
    case SAT_ERR_NOT_FOUND: return "NOT FOUND";
    case SAT_ERR_BUSY: return "BUSY";
    case SAT_ERR_TIMEOUT: return "TIMEOUT";
    case SAT_ERR_NOT_CONNECTED: return "NOT CONNECTED";
    case SAT_ERR_UNFORMATTED: return "UNFORMATTED";
    case SAT_ERR_WRITE_PROTECTED: return "WRITE PROTECTED";
    case SAT_ERR_ALREADY_EXISTS: return "ALREADY EXISTS";
    case SAT_ERR_VERIFY_FAILED: return "VERIFY FAILED";
    case SAT_ERR_VERSION: return "VERSION";
    }
    return "UNKNOWN";
}

/* __FILE__ is whatever path the compiler was given; keep only the last
 * component, handling both separators because the build runs on Windows. */
inline const char* basename(const char* path) {
    const char* base = path;
    if (path == nullptr) return "";
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    return base;
}

inline char upper(char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

/* Appends `src` to `out` at `pos`, upper-cased, never past kLineChars. */
inline uint8_t append(char* out, uint8_t pos, const char* src) {
    while (*src != '\0' && pos < kLineChars) out[pos++] = upper(*src++);
    out[pos] = '\0';
    return pos;
}

inline uint8_t append_i32(char* out, uint8_t pos, int32_t value) {
    char digits[11];
    uint8_t n = 0u;
    uint32_t mag = value < 0 ? 0u - static_cast<uint32_t>(value)
                             : static_cast<uint32_t>(value);
    if (value < 0 && pos < kLineChars) out[pos++] = '-';
    do {
        digits[n++] = static_cast<char>('0' + (mag % 10u));
        mag /= 10u;
    } while (mag != 0u);
    while (n != 0u && pos < kLineChars) out[pos++] = digits[--n];
    out[pos] = '\0';
    return pos;
}

/* Fills the report: title, error name, source file, "LINE n  CODE c".
 * Each row is a kLineChars + 1 byte NUL-terminated string. */
inline void build_report(
    char lines[kLineCount][kLineChars + 1u],
    sat_result_t st, const char* file, int32_t line
) {
    for (uint8_t i = 0u; i < kLineCount; ++i) lines[i][0] = '\0';
    append(lines[0], 0u, "FATAL ERROR");
    append(lines[1], 0u, result_name(st));
    append(lines[2], 0u, basename(file));
    uint8_t pos = append(lines[3], 0u, "LINE ");
    pos = append_i32(lines[3], pos, line);
    pos = append(lines[3], pos, "  CODE ");
    append_i32(lines[3], pos, static_cast<int32_t>(st));
}

/* 3x5 glyphs: five rows, top first, each a 3-bit mask whose highest bit is
 * the leftmost pixel. */
struct Glyph {
    char ch;
    uint8_t rows[5];
};

constexpr Glyph kGlyphs[] = {
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}}, {'2', {7, 1, 7, 4, 7}},
    {'3', {7, 1, 7, 1, 7}}, {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 7, 1, 7}},
    {'6', {7, 4, 7, 5, 7}}, {'7', {7, 1, 1, 1, 1}}, {'8', {7, 5, 7, 5, 7}},
    {'9', {7, 5, 7, 1, 7}},
    {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}}, {'C', {3, 4, 4, 4, 3}},
    {'D', {6, 5, 5, 5, 6}}, {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}},
    {'G', {7, 4, 5, 5, 7}}, {'H', {5, 5, 7, 5, 5}}, {'I', {7, 2, 2, 2, 7}},
    {'J', {1, 1, 1, 5, 2}}, {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}},
    {'M', {5, 7, 7, 5, 5}}, {'N', {6, 5, 5, 5, 5}}, {'O', {2, 5, 5, 5, 2}},
    {'P', {6, 5, 6, 4, 4}}, {'Q', {2, 5, 5, 7, 3}}, {'R', {6, 5, 6, 5, 5}},
    {'S', {3, 4, 2, 1, 6}}, {'T', {7, 2, 2, 2, 2}}, {'U', {5, 5, 5, 5, 7}},
    {'V', {5, 5, 5, 5, 2}}, {'W', {5, 5, 7, 7, 5}}, {'X', {5, 5, 2, 5, 5}},
    {'Y', {5, 5, 2, 2, 2}}, {'Z', {7, 1, 2, 4, 7}},
    {'-', {0, 0, 7, 0, 0}}, {':', {0, 2, 0, 2, 0}}, {'.', {0, 0, 0, 0, 2}},
    {' ', {0, 0, 0, 0, 0}},
};

/* Unknown characters draw as a solid block so a bad byte is visible rather
 * than silently blank. */
inline const uint8_t* glyph_rows(char c) {
    static constexpr uint8_t kUnknown[5] = {7, 7, 7, 7, 7};
    const char u = upper(c);
    for (const Glyph& g : kGlyphs) {
        if (g.ch == u) return g.rows;
    }
    return kUnknown;
}

/* Calls emit(ctx, x, y, w, h) once per horizontal run of lit pixels of
 * `text` laid out from (x, y), each font pixel being scale x scale screen
 * pixels and each cell 4 font pixels wide (3 + 1 gap). Merging runs keeps a
 * whole report to a few hundred VDP1 commands instead of one per pixel. */
template <typename Emit>
inline void for_each_run(
    const char* text, int x, int y, int scale, Emit emit
) {
    for (int cell = 0; text[cell] != '\0'; ++cell) {
        const uint8_t* rows = glyph_rows(text[cell]);
        const int cx = x + cell * 4 * scale;
        for (int row = 0; row < 5; ++row) {
            const uint8_t row_bits = rows[row];
            int col = 0;
            while (col < 3) {
                if ((row_bits & (0x4u >> col)) == 0u) { ++col; continue; }
                int end = col;
                while (end < 3 && (row_bits & (0x4u >> end)) != 0u) ++end;
                emit(cx + col * scale, y + row * scale,
                     (end - col) * scale, scale);
                col = end;
            }
        }
    }
}

}  /* namespace saturn::core::panic */

#endif /* SATURN_CORE_PANIC_LOGIC_HPP */
