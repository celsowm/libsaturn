#ifndef SATURN_PROFILE_H
#define SATURN_PROFILE_H

#include <stdint.h>
#include <stddef.h>

/* Caller-owned, single-threaded nested timing accumulator. Supply monotonic
 * uint32 timestamps in one unit (extended FRT ticks, for example). Unsigned
 * subtraction permits clock wrap; every span must be shorter than 2^32 ticks.
 * Use separate instances per CPU; this API performs no cache publication.
 * Timings include instrumentation and interrupts, not just instruction cost. */
typedef struct sat_profile_counter {
    uint32_t inclusive, exclusive, calls;
} sat_profile_counter_t;
typedef struct sat_profile_span {
    uint32_t started, children, tag;
} sat_profile_span_t;
enum {
    SAT_PROFILE_INVALID = 1u,
    SAT_PROFILE_DEPTH = 2u,
    SAT_PROFILE_ORDER = 4u,
    SAT_PROFILE_OVERFLOW = 8u,
    SAT_PROFILE_CLOCK = 16u
};
typedef struct sat_profile {
    sat_profile_counter_t* counters;
    sat_profile_span_t* stack;
    uint32_t counter_count, stack_capacity, depth, errors;
} sat_profile_t;

static inline int sat_profile_init(sat_profile_t* p,
        sat_profile_counter_t* counters, uint32_t count,
        sat_profile_span_t* stack, uint32_t capacity) {
    if (!p) return 0;
    const sat_profile_t initial = {counters, stack, count, capacity, 0, 0};
    *p = initial;
    if (!counters || !stack || !count || !capacity) {
        p->errors = SAT_PROFILE_INVALID; return 0;
    }
    for (uint32_t i = 0; i < count; ++i) {
        counters[i].inclusive = counters[i].exclusive = counters[i].calls = 0;
    }
    return 1;
}

static inline int sat_profile_enter(sat_profile_t* p, uint32_t tag,
                                     uint32_t now) {
    if (!p || p->errors) return 0;
    if (tag >= p->counter_count) { p->errors |= SAT_PROFILE_INVALID; return 0; }
    if (p->depth >= p->stack_capacity) { p->errors |= SAT_PROFILE_DEPTH; return 0; }
    const sat_profile_span_t span = {now, 0, tag};
    p->stack[p->depth++] = span;
    return 1;
}

static inline int sat_profile_leave(sat_profile_t* p, uint32_t tag,
                                     uint32_t now) {
    if (!p || p->errors) return 0;
    if (!p->depth || p->stack[p->depth - 1].tag != tag) {
        p->errors |= SAT_PROFILE_ORDER; return 0;
    }
    const sat_profile_span_t span = p->stack[p->depth - 1];
    const uint32_t elapsed = now - span.started;
    if (span.children > elapsed) { p->errors |= SAT_PROFILE_CLOCK; return 0; }
    sat_profile_counter_t* counter = &p->counters[tag];
    if (UINT32_MAX - counter->inclusive < elapsed ||
        UINT32_MAX - counter->exclusive < elapsed - span.children ||
        counter->calls == UINT32_MAX ||
        (p->depth > 1 && UINT32_MAX - p->stack[p->depth - 2].children < elapsed)) {
        p->errors |= SAT_PROFILE_OVERFLOW; return 0;
    }
    counter->inclusive += elapsed;
    counter->exclusive += elapsed - span.children;
    ++counter->calls;
    --p->depth;
    if (p->depth) p->stack[p->depth - 1].children += elapsed;
    return 1;
}

/* Incomplete or failed captures must never be presented as complete timings. */
static inline int sat_profile_complete(const sat_profile_t* p) {
    return p && !p->errors && !p->depth && p->counter_count && p->stack_capacity;
}
#endif
