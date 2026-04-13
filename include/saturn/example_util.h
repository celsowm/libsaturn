#ifndef SATURN_EXAMPLE_UTIL_H
#define SATURN_EXAMPLE_UTIL_H

#include "saturn/core.h"

static inline void sat_example_must(sat_result_t st) {
    if (st != SAT_OK) {
        for (;;) {
        }
    }
}

#endif /* SATURN_EXAMPLE_UTIL_H */
