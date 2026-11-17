# B3 V1 — exact weighted delta experiment

The existing deform path already uses MAC.L for each transform and has
a dedicated exact half-weight path. Leave those untouched. For validated
uneven pairs, rewrite floor((w*a+(32768-w)*b+16384)/32768) as
b+floor((w*(a-b)+16384)/32768). Three influences use c as the base and
two delta products. Signed-32 subtraction overflow is detected before
using the delta; the affected axis falls back to the original widened
formula. SH-2 uses DMULS.L, ADDC and extraction of bits 15..46 via
SHLL/ROTCL/XTRCT. No SR.S changes, no DIVU, no asset layout changes.
The existing transform path still requires SR.S clear. Source-level
fixed-point-safe transform inputs are unchanged. No production dispatch.

Compare two public-shaped range entry points in one binary, using the
actual optimized transform and existing rigid/single/half behavior.
64 rounds, 12 cases, three scheduling lanes, two emulators. Independent
widened quotient/remainder oracle; exact output and untouched guard/
out-of-range records checked. Include extreme translations, weights
0/32768, non-Q15 positions, mixed records, partial ranges, rejected
ranges, and separate full-range weighted math checks. Report observed
uneven/triple and overflow fallback axis counts. Timed intervals include
whole range deformation, not oracle, input construction or guard checks.
The real-asset distribution and renderer A/B remain next gates; do not
assume savings before measurements.

Run from the repository root:
`python3 harness/scripts/run_weighted_blend.py`

Return `harness/build/weighted_blend_bench-return.zip` with both reports,
Ymir diagnostic HUD and local-changes snapshot. No need to modify local
source. Target compilation and measurements pending.

## V1 target results — 2026-11-17

Checkout `5a891d7fe8272c7f3cdf88f691e50b04865d185b`. The combined
return archive and standalone Mednafen archive contain identical
Mednafen entries; count that execution only once. The local-changes
manifest is complete, status is main tracking origin/main, all three
tracked patches are empty, and no nonignored untracked files are listed.

Both 1568-byte big-endian reports were independently decoded against
the JSON. All three lanes on each emulator completed; init/parallel
status 0, slave-ready 1, FRT 210540 Hz, CCR 1, SR.S clear at entry.
Across both emulators: 4608 scenarios, 350208 output vertices per
implementation, 24192 extra edge checks, zero recorded errors.
Oracle-classified weighted axes: 190080 pairs, 408960 triples, 73728
overflow fallbacks (all in the extreme workload). Ymir 4800 fields,
zero reported misaligned accesses on both CPUs. Mednafen 1.32.1 native
CD, data-only cache, return code 0, swapped16 WRAM normalized against
ELF text; ELF SHA256
`306874d71268714f3454ee29a16cea57b95f956a5701f6924264bd8d9e704cb3`.
Ymir ELF identity and explicit panic records were not included.

| Scenario | Ymir control / candidate FRT sum | Mednafen control / candidate FRT sum | Time reduction Ymir / Mednafen |
|---|---:|---:|---:|
| empty | 90 / 81 | 122 / 105 | 10.00% / 13.93% |
| singleton | 311 / 288 | 367 / 327 | 7.40% / 10.90% |
| rigid | 5513 / 5589 | 5490 / 5609 | -1.38% / -2.17% |
| single | 6202 / 6624 | 6354 / 6722 | -6.80% / -5.79% |
| half | 15238 / 14412 | 15208 / 14327 | 5.42% / 5.79% |
| uneven_pair | 17794 / 15761 | 17070 / 15396 | 11.43% / 9.81% |
| triple | 20772 / 24026 | 19415 / 23244 | -15.67% / -19.72% |
| mixed | 11709 / 12096 | 11289 / 11861 | -3.31% / -5.07% |
| q12 | 27549 / 30659 | 26236 / 29974 | -11.29% / -14.25% |
| q0 | 26252 / 29366 | 24932 / 28667 | -11.86% / -14.98% |
| partial | 7021 / 7240 | 6739 / 7085 | -3.12% / -5.13% |
| extreme | 14931 / 16821 | 14304 / 16330 | -12.66% / -14.16% |

Serial Master sums over 64 calls; positive means less elapsed time.
Uneven-pair reductions across all three lanes: Ymir 11.43–11.45%,
Mednafen 9.81–12.05%. Triple regressions across lanes are consistent.
Because rigid/single/half paths are semantically unchanged, their
timing differences show compiler/code layout effects; isolated
instruction-level attribution is not established. The fallback counts
are oracle predictions, not independent branch instrumentation.

**Disposition:** mathematical gate PASS; V1 blanket optimization
REJECTED for performance. Retain as an experiment, not default.
Next isolate a pair-only candidate, leaving triple and other cases
on the original path, and profile actual asset weight distribution
before deciding whether a real-character A/B is worthwhile. Explore
a separate triple schedule only if justified by workload coverage.
Require no meaningful mixed-record or fallback regression, and
measure actual consumer performance and pixels before activation.
B1/B2 remain independent. No physical hardware, explicit interrupt
stress, projected range or placed range qualification in this run.

## V2 selective experiment

Retain V1 and the original control in one binary. V2 changes only unequal
two-weight blends, leaves the original three-weight arithmetic in place,
and uses SH-2 SUBV/MOVT to detect signed delta overflow (with the original
64-bit fallback). Rigid/single/half remain semantically unchanged; code
layout can still affect timing. All three outputs (including untouched
guard records) are compared against the same oracle, same input seeds and
12 scenarios as V1. Rotate six call orders. Additional edge checks cover
the selective pair itself (4480 total per lane). Report version 2 retains
the 1568-byte layout and uses the former row reserved word for selective
FRT ticks. Fallback axes continue to describe V1's oracle predictions;
on V2, triple axes are not optimized at all.

The runner also generates a weight inventory for the tracked Robot Police
snapshot, any existing locally generated robot, and fresh procedural
fighter high/medium/low exports. Include source hashes, per-kind record
counts, run lengths and joint locality. Missing assets/generation errors
are explicit, not interpreted as zero weights. The real renderer and
projected/placed path qualification are still pending.

Run `python3 harness/scripts/run_weighted_blend.py` and return
`harness/build/weighted_blend_bench-v2-return.zip`. Includes local
changes, diagnostics and asset-weights.json. No production activation.
