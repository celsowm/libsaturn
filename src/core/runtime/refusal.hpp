#ifndef SATURN_CORE_REFUSAL_HPP
#define SATURN_CORE_REFUSAL_HPP

/* Where the last refusal came from (SAT_REFUSE in saturn/core.h). A result
 * code usually travels up through several SAT_TRY frames before the caller's
 * SAT_PANIC_IF_ERROR stops: the panic screen then names the caller's line,
 * and this record names the check that actually refused. Header-only, so
 * every library source and host test shares one object without linking
 * panic.cpp. Either CPU may write it: a torn record only mislabels a report. */

#include <stdint.h>

#include "saturn/core.h"

namespace saturn::core {

struct Refusal {
    const char* file;
    int32_t line;
    int32_t code;
};

inline volatile Refusal g_refusal = {nullptr, 0, 0};

inline sat_result_t refuse(sat_result_t st, const char* file, int line) {
    g_refusal.file = file;
    g_refusal.line = line;
    g_refusal.code = static_cast<int32_t>(st);
    return st;
}

}  // namespace saturn::core

/* Library-internal spelling of SAT_REFUSE, without the C call. */
#define SAT_REFUSE_HERE(st) ::saturn::core::refuse((st), __FILE__, __LINE__)

#endif /* SATURN_CORE_REFUSAL_HPP */
