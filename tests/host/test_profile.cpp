#include "saturn/profile.h"
#include <cassert>
#include <cstdio>

int main() {
    sat_profile_t p;
    sat_profile_counter_t counters[3];
    sat_profile_span_t stack[3];
    assert(sat_profile_init(&p, counters, 3, stack, 3));
    assert(sat_profile_enter(&p, 0, UINT32_MAX - 9));
    assert(sat_profile_enter(&p, 1, UINT32_MAX - 4));
    assert(sat_profile_enter(&p, 1, 0)); // recursive tags, wrapped clock
    assert(sat_profile_leave(&p, 1, 2));
    assert(sat_profile_leave(&p, 1, 5));
    assert(sat_profile_leave(&p, 0, 10));
    assert(sat_profile_complete(&p));
    assert(counters[0].inclusive == 20 && counters[0].exclusive == 10);
    assert(counters[1].inclusive == 12 && counters[1].exclusive == 10);
    assert(counters[1].calls == 2 && counters[2].calls == 0);
    assert(counters[0].exclusive + counters[1].exclusive == counters[0].inclusive);

    assert(sat_profile_init(&p, counters, 3, stack, 1));
    assert(sat_profile_enter(&p, 0, 0));
    assert(!sat_profile_complete(&p));
    assert(!sat_profile_enter(&p, 1, 1) && p.errors == SAT_PROFILE_DEPTH);
    assert(!sat_profile_leave(&p, 0, 2)); // failed capture stays failed
    assert(sat_profile_init(&p, counters, 3, stack, 3));
    assert(!sat_profile_enter(&p, 3, 0) && p.errors == SAT_PROFILE_INVALID);
    assert(sat_profile_init(&p, counters, 3, stack, 3));
    assert(sat_profile_enter(&p, 0, 0));
    assert(!sat_profile_leave(&p, 1, 1) && p.errors == SAT_PROFILE_ORDER);
    assert(sat_profile_init(&p, counters, 3, stack, 3));
    assert(!sat_profile_leave(&p, 0, 0) && p.errors == SAT_PROFILE_ORDER);
    assert(sat_profile_init(&p, counters, 3, stack, 3));
    assert(sat_profile_enter(&p, 0, 0));
    assert(sat_profile_enter(&p, 1, 1));
    assert(sat_profile_leave(&p, 1, 5));
    assert(!sat_profile_leave(&p, 0, 3) && p.errors == SAT_PROFILE_CLOCK);
    assert(sat_profile_init(&p, counters, 3, stack, 3));
    counters[0].inclusive = UINT32_MAX;
    assert(sat_profile_enter(&p, 0, 0));
    assert(!sat_profile_leave(&p, 0, 1) && p.errors == SAT_PROFILE_OVERFLOW);
    assert(!sat_profile_init(&p, nullptr, 0, stack, 3));
    assert(!sat_profile_complete(&p));
    assert(!sat_profile_init(nullptr, counters, 3, stack, 3));
    puts("nested profiler: exclusive accounting, recursion, wrap and failure gates passed");
}
