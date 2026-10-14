# Killer Clown measured performance baseline

The first profiling step of the
[optimization plan](KILLER_CLOWN_OPTIMIZATION_PLAN.md) is implemented.
The imported fight's largest measured stage is lighting, followed by scene
geometry preparation. This establishes an optimization baseline; it does not
claim a frame-rate improvement or achievement of the 30-image/s target.

## Workloads and measurement

`harness/run-clown-profile.ps1` builds the example and runs three fixed NTSC
input timelines on Ymir with no RAM cartridge. Each run lasts 1,000 program
fields after BIOS initialization. A ring of 192 records in WRAM-L stores
extended-FRT stage durations, VBlank/loop/simulation identifiers, light/UI
state, command count and late swaps. The record is 92 bytes; the complete
ring with header occupies 17,688 bytes.

The analyzer discards two seconds after the first fight sample and selects
the following ten-second window. Boundary spacing means the measured
first-to-last sample span is slightly shorter than ten seconds. It requires
at least nine seconds of coverage and 20 steady samples, rejects panics,
checks record layout and validates parent/child timing relationships.

The idle timeline holds no player input; the CPU opponent remains active.
Walk alternates right/left movement. Combat holds punch, kick, jump and guard
in fixed field intervals. These are reproducible application workloads, not
frozen-pose microbenchmarks. Rendering cadence affects the poses sampled.

## Results, 2026-10-14

All three timing gates pass. Values below are medians in milliseconds.

| Measurement | Idle input | Walk input | Combat input |
| --- | ---: | ---: | ---: |
| Steady samples | 56 | 57 | 57 |
| Submitted loops/s | 5.670 | 5.744 | 5.744 |
| Simulation ticks/s | 60.000 | 60.000 | 59.897 |
| Complete loop, including waits | 180.150 | 169.535 | 173.077 |
| VBlank/begin-frame wait | 11.341 | 6.885 | 10.817 |
| Input, simulation and audio servicing | 1.087 | 1.187 | 0.978 |
| Pose, skeleton, skinning and bounds | 25.430 | 25.394 | 22.982 |
| Camera, fence and floor updates | 3.186 | 3.196 | 3.186 |
| Complete body lighting | 80.572 | 78.827 | 81.258 |
| Normal accumulation, inside lighting | 25.109 | 25.123 | 25.128 |
| Scene preparation, including light geometry | 38.549 | 37.982 | 38.352 |
| Scene flush | 15.596 | 15.361 | 15.503 |
| Painter sorting, inside flush | 4.311 | 4.217 | 4.288 |
| Command emission, inside flush | 9.789 | 9.653 | 9.734 |
| HUD | 0.907 | 0.902 | 0.907 |
| End-frame submission | 2.035 | 1.980 | 2.023 |

Full p95/max values and individual records are in
`harness/build/clown_profile_{idle,walk,combat}_profile.json`.
The idle complete-loop p95 is 186.268 ms and maximum is 189.088 ms.
Its steady interval adds seven late swaps. Late swaps retain the previous
image; the guest does not report a panic in any workload.

## Interpretation and next optimization

Lighting alone exceeds the complete 33.3 ms target budget. About 25 ms of
that stage accumulates normals from both deformed meshes; the remaining work
includes normal normalization, world-space vertex conversion, two spotlight
evaluations, Gouraud encoding and diagnostic visibility calculations. The
current record does not separate those remaining components.

Start the next change with exact lighting reductions and a true ambient-only
fast path. Follow with finer measurement of normalization versus spotlight
evaluation, dirty-state reuse and numerical tests for any bounded arithmetic
replacement. Keep texture pixels, CLUTs and the reference lighting path intact.
Then apply the existing paired geometry adapter with serial equivalence checks.
Neither removing the fog geometry alone nor optimizing the three-millisecond
floor update is supported as the primary fix by this profile.

## Limits and timing ownership

These are emulator measurements of submitted application loops, not unique
displayed-image FPS or physical Saturn acceptance. A nominal 60 Hz simulation
does not prove 60 Hz input sampling: the loop still uses one pad snapshot
while catching up multiple simulation ticks.

`evaluate` contains `pose_dispatch`, `skin_join` and `bounds`. Pose/dispatch
includes synchronous Master skin work; join includes residual wait/result
consumption/release, not the complete parallel skin elapsed time. Lighting
contains normal accumulation. Flush contains sorting/emission. Never add
parent and child stages, or independently computed medians, as a decomposition
of one frame. The outer stage clock is extended; existing renderer internal
sort/emission counters remain 16-bit and are suitable only for short stages.

No per-frame allocation, logging to disk or texture upload is added. Timing
reads and telemetry writes do add work. `CLOWN_PROFILE=0` removes the example's
per-loop instrumentation while retaining the same library profiling build;
the runner includes a whole-program comparison at identical input and field
duration. That comparison is a coarse scheduling check, not an exact estimate
of per-stage instrumentation cost. Sustained image-presentation tracking,
PAL profiling and physical hardware acceptance remain open.

The matched 1,000-field idle comparison completes 92 loops with instrumentation
and 92 without it, with zero panic, body overlap, draw error or timeout in the
no-record run. No whole-loop throughput loss is resolved by this comparison;
this does not imply that timestamp reads have zero cost. Results are in
`harness/build/clown_profile_no_record_comparison.json`.

## Baseline validation and memory

Both profile/no-record ISO builds pass the existing CMOK/IP/disc-image checks.
The final instrumented 700-field smoke completes 63 submissions and 657
simulation ticks with zero panic, body overlap, draw error, VDP1 timeout or
audio queue failure. Seven late swaps retain the previous image. The 68000
executes 31 combat sound events and captured audio peaks at 5,135/32,767.
The final Mednafen disc boot and playable fight are captured in
`harness/build/clown_profile_mednafen.png`.

The final map ends WRAM-H BSS at `0x060fa568`, leaving 6,808 bytes beyond the
reserved 16 KiB Master stack. WRAM-L usage ends at `0x0023d75c` (251,740 bytes).
Future geometry buffers belong in a separately budgeted WRAM-L region;
stock WRAM-H remains tight. Python runtime analysis and `git diff --check`
pass. No hardware acceptance is inferred from these software gates.

## Exact lighting arithmetic follow-up

The retained follow-up changes the example-local vector normalization and
ambient mode. Meshes, animation sampling, texture pixels, RGB CLUTs, cone
attenuation and combat rules retain their existing behavior.

Normalization first determines the unsigned maximum component's required
shift. It applies the shift once and checks whether negative arithmetic
rounding requires one additional bit. This produces the same bounded vector
as the preceding repeated three-component loop. The unsigned magnitude also
handles `INT32_MIN` without a signed negation overflow.

A lazily initialized 2,048-entry reciprocal table occupies 8 KiB in WRAM-L.
For component magnitude `N <= 2^26` and length `d`, the estimate is
`floor(N * floor((2^32 - 1) / d) / 2^32)`. Its quotient underestimates by at
most one; a remainder check supplies the missing unit. Restoring the sign
gives exactly the preceding integer division, including truncation toward
zero. The normalized vector's length is at most 1,773, within the table.
The table is initialized on Master before steady-state lighting and is not
a thread-safe worker interface. The emitted SH-2 code uses `DMULU.L` for
the reciprocal product; steady normalization avoids the three software divides.

The lighting loop calculates each vertex's camera-facing sign once for both
spots. Ambient mode fills constant costume Gouraud corrections directly and
skips normals, cone aiming and per-vertex spotlight evaluation. Re-enabling
lights evaluates the current deformed mesh; no stale normal cache is used.

Host tests compare 200,000 vectors and additional shift-boundary cases with
the previous integer implementation, then check the minimum signed component,
cone coverage and floor intersections. The shared two-cube example uses the
same helper.

A separate attempt to replace face cross-product scaling with variable
64-bit shifts raised normal accumulation from 25 to 38 ms on Ymir and was
removed. The library's original face-normal algorithm is retained. That
rejected experiment is archived in `harness/build/clown_lighting_rejected_cross`;
it is not included in the final performance comparison.

Matched input/reference files are archived under
`harness/build/clown_lighting_before`. The final runner writes
`harness/build/clown_lighting_final_*`; the comparison tool checks matching
pad scripts and timer calibration before comparing stage medians. These are
matched workloads rather than identical sampled poses: the render cadence
changes while simulation uses elapsed time. Submission gains remain separate
from unique displayed-image FPS.

| Workload | Before / after lighting ms | Before / after loop ms | Before / after submissions/s |
| --- | ---: | ---: | ---: |
| Idle input | 80.572 / 73.834 | 180.150 / 166.296 | 5.670 / 5.990 |
| Walk input | 78.827 / 71.702 | 169.535 / 166.849 | 5.744 / 5.970 |
| Combat input | 81.258 / 74.065 | 173.077 / 165.632 | 5.744 / 6.040 |

Lighting medians improve by 8.36–9.04%; sustained submission throughput
improves by 3.93–5.64%. This is a modest first reduction, not acceptance of
the 30-image/s target. Normal accumulation remains about 25 ms and scene
preparation about 38 ms. Subsequent work should isolate the remaining
normalization/spotlight costs and reuse/parallelize appropriate preparation.

The matched no-record build and instrumented build each complete 97 loops
over 1,000 fields. As before, this does not resolve timestamp overhead below
the scheduling granularity. The ambient-only gate measures 0.128 ms median
lighting, zero normal accumulation and 11.619 submitted loops/s. Its final
VDP1 list has 462 fighter Gouraud faces, each containing the correct uniform
costume correction, and no additive light sprites. This validates the new
fast path; it is a different visual mode from the full-lighting benchmark.

`harness/tests/clown_profile.py --compare-before` produces
`harness/build/clown_lighting_final_comparison.json`. The telemetry decoder
now names the enabled flag `lights_enabled`, distinguishing it from the
`lights` duration; the guest wire layout is unchanged.

Final validation passes the full host/tool suite, both ISO builds, the
360-field cube gate and the 2,300-field fight gate. The cubes complete 355
loops with three reference faces, three Gouraud faces, one additive pool,
zero errors and identical paused images. The fight completes 231 submissions
and 1,755 simulation ticks with zero panic, body overlap, draw error or VDP1
timeout. Menus, both controls, all fence modes and pause equivalence pass.
The 68000 executes 74 sound events with zero queue failures and maximum
lateness one tick; captured audio peaks at 5,174/32,767. Its 57 late swaps
retain previous images and remain a presentation limitation, not a failed
draw or proof of unique image FPS.

The final instrumented map ends WRAM-H BSS at `0x060fa7b8`, leaving 6,216 bytes
beyond the reserved 16 KiB Master stack. WRAM-L usage is 259,932 bytes, including
the additional 8 KiB table. The default Mednafen disc tests pass for both
examples without a RAM cartridge; inspected captures are
`harness/build/clown_lighting_mednafen.png` and
`harness/build/clown_lighting_cube_mednafen.png`.

## Paired geometry preparation follow-up

The example now uses the existing scene preparation adapter for the second
fighter while Master prepares the first fighter and the shadows. Both skin
jobs are joined and released before dispatch. The adapter publishes immutable
mesh, transform, texture and Gouraud inputs, gives the worker private scratch,
and consumes the output after completion. Master merges in the original
stage/shadow/actor order, then performs the single painter sort and command
emission. No VDP1 register or command writes occur on Slave.

The optional verification build repeats serial preparation after joining and
merging each worker result. It compares admitted/culled/clipped counts, depth
keys, projected coordinates, every material field, Gouraud values and world
coordinates for clipping fallbacks. Unspecified world storage on safe faces
is excluded. Verification runs are correctness gates, not speed benchmarks.
The normal build retains a serial fallback when Slave is unavailable; an
explicit `CLOWN_PARALLEL_GEOMETRY=0` build forces the serial implementation.

The additional 600-face/key buffers occupy 64,800 bytes (63.281 KiB) in WRAM-L.
The default map ends WRAM-L at `0x0024f47c` (324,732 bytes used), and WRAM-H BSS
at `0x060fb198`, leaving 3,688 bytes beyond the reserved 16 KiB Master stack.
These are linker budgets, not a measured runtime stack high-water mark.

Matched profiles use the unchanged 1,000-field idle/walk/combat input scripts
from the exact-lighting follow-up (`harness/build/clown_lighting_final_*`).
The new runs and paired comparison are in
`harness/build/clown_geometry_parallel_*`. The comparison checks identical
input bytes and timer calibration. Sampled poses may differ with cadence;
these rates count submitted loops, not unique displayed images.

| Workload | Serial / paired geometry ms | Serial / paired loop ms | Serial / paired submissions/s |
| --- | ---: | ---: | ---: |
| Idle input | 38.526 / 26.534 | 166.296 / 152.073 | 5.990 / 6.407 |
| Walk input | 37.892 / 26.458 | 166.849 / 151.220 | 5.970 / 6.476 |
| Combat input | 38.257 / 26.586 | 165.632 / 151.662 | 6.040 / 6.486 |

Geometry medians improve by 30.18–31.13%; sustained submission rates improve
by 6.96–8.48%. Lighting remains about 74 ms and accumulated face normals about
25 ms. The 30-image/s target remains open. This change adds no library API,
new LOD, texture conversion or lighting approximation.

The 2,300-field verification fight passes all menus, fence modes, paused-image
equivalence and both pads. All 222 completed pairs match the serial reference.
There are zero mismatches, panic, body overlaps, draw errors, VDP1 timeouts
or audio queue failures. The 68000 executes all 76 requested sound events,
with maximum lateness zero ticks and captured peak 6,773/32,767. Forty late
swaps retain previous images. Its 222 submissions include repeated reference
work and must not be used as the normal build's performance result. Details
are in `harness/build/killer_clown_fight_analysis.json` and
`harness/build/clown_geometry_verify_fight.log`.

The existing scene preparation host API test passes. Serial-only, verification
and default ISO builds pass CMOK/IP/disc-image validation. The verification
build also boots from disc without a RAM cartridge in Mednafen and accepts
punch/kick input; its inspected capture is
`harness/build/clown_geometry_verify_mednafen.png`. The default binary is
restored after the verification and serial-only builds. Physical Saturn/
Satiator acceptance and runtime stack high-water measurements remain open.

The restored default build also passes Mednafen disc boot and punch/kick input
without a RAM cartridge. The inspected final capture is
`harness/build/clown_geometry_mednafen.png`.

## Parallel lighting and narrow normal arithmetic

The shared cross-product scaler now shifts 64-bit components only until the
maximum magnitude fits `INT32_MAX`. It then performs the remaining per-bit
shifts on 32-bit components. This keeps the previous shift count and signed
rounding exactly: after each shift, a negative component's magnitude can be
at most `m+1`, which still fits `int32_t` when `m <= INT32_MAX`. The 20-bit
final scaling limit is unchanged. Host tests compare 100,000 vector pairs,
power-of-two boundaries, degenerate inputs and extreme axis-aligned products
against the preceding wide-shift implementation.

An isolated serial-lighting run with this arithmetic change measures 23.799 ms
normal accumulation, compared with 25.114 ms in the preceding idle profile.
Its 6.497 submissions/s are a modest improvement over 6.407; the larger
optimization below comes from overlapping independent work.

The arena uses two synchronous `sat_parallel_for` ranges: first both actors'
normal accumulation, then both actors' per-vertex spotlight/Gouraud evaluation.
Each range gives Master one actor and Slave the other, and joins before the
next phase. Existing skin jobs are already released; geometry dispatch begins
only after lighting has joined. No worker writes graphics registers or scene
commands. Source poses, lights and camera remain immutable during the ranges.
The range adapter purges Slave's input cache and consumes the second actor's
bounded output span on Master. Each actor owns its normal, Gouraud and light
counter storage. Global telemetry is combined only after completion.

Master aims both nondegenerate spotlights before dispatch. This also primes
the existing reciprocal table; workers read it only after initialization and
cache publication. Concurrent first-use table initialization remains
unsupported. Ambient mode bypasses both ranges. Disabling the parallel option
or running without Slave uses the same functions serially.

`PROFILE_NORMALS` still measures elapsed wall time within `PROFILE_LIGHTS`,
now including both normal ranges' preparation/join rather than two sequential
calls. It is not a sum of CPU time across the processors. The wire format
and existing 60 Hz combat rules remain unchanged.

The optional `CLOWN_VERIFY_LIGHTING=1` build preserves the second actor's
actual output after joining, recomputes the same pose on Master, and compares
all normal vectors, Gouraud words, visibility/strength counters and status
bytes. Its 4,248-byte reference buffer lives in WRAM-L. The runtime analyzer
requires every enabled verification update to match and rejects errors.

The three 1,000-field NTSC workloads retain the preceding paired-geometry
input scripts. Results are under `harness/build/clown_parallel_lighting_*`;
the comparison uses `harness/build/clown_geometry_parallel_*` as baseline.
The walk and combat probes run independently against the same immutable disc
and application. Emulated guest-clock measurements are independent of host
wall time and are not host emulator FPS.

| Workload | Before / after lighting ms | Before / after loop ms | Before / after submissions/s |
| --- | ---: | ---: | ---: |
| Idle input | 73.813 / 38.273 | 152.073 / 116.496 | 6.407 / 8.499 |
| Walk input | 72.635 / 38.378 | 151.220 / 116.652 | 6.476 / 8.542 |
| Combat input | 73.851 / 38.444 | 151.662 / 116.586 | 6.486 / 8.499 |

Lighting medians fall 47.16-48.15%; sustained submissions increase 31.04-32.65%.
Normal-stage elapsed time falls from about 25.1 to 12.1 ms. This combines
narrow arithmetic with overlapping the two actors; it does not halve the
number of normals computed. Actual displayed-image FPS and the 30-image/s
acceptance target remain open.

The old measured timer calibration is 210,600 Hz and the new one 210,540 Hz.
The difference is exactly one FRT count per NTSC field, or 0.0285%. Each
report converts ticks with its own calibration. The comparison now requires
calibrations to be multiples of 60 and differ by at most one such count;
it records both frequencies and rejects larger differences. This bounded
quantization difference is far smaller than the measured lighting reduction.
Input bytes must still match exactly.

The full host/tool suite and 360-field cube gate pass. The cube gate completes
355 loops with three reference faces, three Gouraud faces, one additive pool,
zero errors and identical paused images. The default arena map ends WRAM-H
BSS at `0x060fb218`, leaving 3,560 bytes beyond the reserved 16 KiB Master
stack; WRAM-L remains 324,732 bytes. The combined verification build ends
BSS at `0x060fb4c8` and WRAM-L at `0x00250514`, including the 4,248-byte
reference. These are linker budgets, not measured stack high-water marks.

The combined 2,300-field verification gate passes 214 full-light updates and
222 geometry pairs with zero mismatches. Ambient updates intentionally skip
the lighting comparison. Menus, all fence modes, paused-image equivalence and
both controls pass. There are zero panics, body overlaps, draw errors, VDP1
timeouts or audio queue failures. All 76 sound events execute on the 68000,
with maximum lateness zero ticks and peak 6,773/32,767. Its 38 late swaps
preserve previous images. The 222 submitted loops include both serial
recomputations and are excluded from normal-build throughput claims.
See `harness/build/killer_clown_fight_analysis.json` and
`harness/build/clown_parallel_lighting_verify_fight.log`.

Serial-only and default builds pass CMOK/IP/disc checks. The combined
verification binary boots from disc in Mednafen without a RAM cartridge,
accepts punch/kick input and completes a CPU round without a fatal.
Its inspected capture is
`harness/build/clown_parallel_lighting_verify_mednafen.png`. The final build
restores parallel lighting/geometry and disables both expensive comparisons.
Physical hardware acceptance, true presentation counting, dirty-state reuse
and further geometry/lighting costs remain open.

The restored default disc also boots and accepts punch/kick input in Mednafen
without a RAM cartridge. Its inspected capture is
`harness/build/clown_parallel_lighting_mednafen.png`.
