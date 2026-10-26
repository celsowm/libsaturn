# Caller-owned nested profiling

`saturn/profile.h` is a standalone C11/C++ header. It accumulates inclusive
ticks, exclusive ticks and completed calls per application-defined tag. It
does not allocate, read hardware, initialize an executor or publish caches.
Use one instance per CPU/thread and caller-owned counters and stack storage.

```c
#include <saturn/profile.h>
#include <saturn/time.h>

sat_profile_t profile;
sat_profile_counter_t counters[8];
sat_profile_span_t stack[16];
sat_profile_init(&profile, counters, 8, stack, 16);
sat_profile_enter(&profile, 0, (uint32_t)sat_time_ticks());
/* Work; nested enter/leave pairs may use other tags or recurse. */
sat_profile_leave(&profile, 0, (uint32_t)sat_time_ticks());
if (sat_profile_complete(&profile)) {
    /* Consume counters; convert using sat_time_frequency_hz(). */
}
```

Supply monotonic timestamps from one clock in one unit. Unsigned subtraction
handles a single 32-bit wrap, provided each span is shorter than 2^32 ticks.
Do not pass raw wrapping 16-bit FRT values as extended timestamps. Clock
frequency and capture metadata belong to the consumer. Use `sat_time_ticks`
on Saturn and a synthetic clock for host verification.

Each completed span subtracts only its direct children's inclusive duration
from its exclusive duration. Recursive calls with the same tag accumulate
their inclusive costs repeatedly, as expected; use different tags to separate
an outer pipeline from its inner kernel. For one root, the sum of all exclusive
costs equals that root's inclusive duration. Inclusive costs must not be added
as independent savings. Capture overhead and interrupts are included; an
uninstrumented build remains mandatory for cadence measurements.

Invalid tags, exhausted stack, mismatched exits, impossible child durations
and counter overflow set sticky error flags. Further operations fail until
reinitialization. `sat_profile_complete` also rejects an unclosed stack. Never
present a failed or incomplete capture as valid timing data. Counters are
32-bit; reset or drain bounded capture windows before they overflow. The
recorder is not atomic and must not be shared with another CPU or IRQ handler.

The host gate checks nested and recursive accounting, clock wrap, stack limits,
bad tags, mismatched/empty exits, impossible timestamps and overflow. The
installed C package consumer also compiles and exercises the public contract.
Tekken's per-tick recorder and game-specific stage IDs remain in its consumer
repository; other applications can use their own tags and capture layout.
