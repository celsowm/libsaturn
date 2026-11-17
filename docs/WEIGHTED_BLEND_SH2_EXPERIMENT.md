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

## V2 target results — 2026-11-17

Checkout `16a31792bb1bb6b0304a1adf13657f3f5d6d25f8`. Both 1568-byte
telemetry records independently decoded and matched to all report fields.
Standalone Mednafen return is byte-identical to the corresponding combined
archive entries. Local changes manifest complete: tracking main, empty
staged/unstaged/combined patches, no nonignored untracked files.

4608 scenarios, 350208 vertices per implementation, 26880 additional
edge checks, zero errors. Three completed lanes per emulator, init and
parallel status zero, slave available, SR.S clear on lane entry, CCR 1,
210540 Hz. Oracle classified 190080 unequal pair axes, 408960 triple
axes, 73728 V1 fallback axes. Of these fallbacks 36864 are pair axes
relevant to V2 (extreme scenario only). Ymir reported no misaligned
accesses, completed target unittest. Mednafen native CD exit zero,
data-only cache, verified swapped16 WRAM, ELF SHA256
`ad4dcccd90bb36dbd7f026a1359e8a73ecf0fe97aef21097f885c7ca66f903e3`.
No retained Ymir ELF identity or explicit panic dump.

### Serial Master FRT sums (64 timed calls)

| Workload | Ymir control / V1 / V2 | Mednafen control / V1 / V2 | V2 reduction Ymir / Mednafen |
|---|---:|---:|---:|
| empty | 94 / 82 / 83 | 129 / 114 / 99 | 11.70% / 23.26% |
| singleton | 313 / 277 / 277 | 376 / 322 / 322 | 11.50% / 14.36% |
| rigid | 5506 / 5589 / 5580 | 5488 / 5597 / 5189 | -1.34% / 5.45% |
| single | 6194 / 6607 / 6226 | 6277 / 6657 / 6182 | -0.52% / 1.51% |
| half | 15234 / 14406 / 14542 | 15098 / 14253 / 14218 | 4.54% / 5.83% |
| uneven_pair | 17780 / 15750 / 15552 | 16967 / 15312 / 15267 | 12.53% / 10.02% |
| triple | 20765 / 24018 / 21467 | 19308 / 23165 / 19759 | -3.38% / -2.34% |
| mixed | 11707 / 12099 / 11401 | 11216 / 11822 / 10855 | 2.61% / 3.22% |
| q12 | 27536 / 30664 / 27958 | 26130 / 29885 / 26199 | -1.53% / -0.26% |
| q0 | 26244 / 29354 / 26664 | 24825 / 28590 / 24892 | -1.60% / -0.27% |
| partial | 7024 / 7246 / 6776 | 6712 / 7066 / 6472 | 3.53% / 3.58% |
| extreme | 14935 / 16834 / 15074 | 14212 / 16240 / 14365 | -0.93% / -1.08% |

Uneven pair reductions across lanes: Ymir 12.53–12.56%, Mednafen
10.02–11.04%. Mixed 2.56–2.61% and 3.22–3.38%. Residual triple
regression 3.37–3.38% and 2.18–2.34%. Semantically unchanged paths
still exhibit code generation/layout effects; no guarantee of zero
regression. Q12/Q0 cases contain triples only and do not measure
non-Q15 unequal pair speed. Direct V1/V2 incremental improvement is
small; the dominant benefit is avoiding V1 triple regression.

### Exported asset distribution (inventory errors: none)

| Asset | Vertices | Rigid | Half | Unequal pair | Triple | Pair share | Pair runs / max run |
|---|---:|---:|---:|---:|---:|---:|---:|
| Robot tracked (Q10) | 943 | 241 | 0 | 326 | 376 | 34.57% | 83 / 22 |
| Robot existing local generated (Q10) | 933 | 210 | 0 | 391 | 332 | 41.91% | 97 / 32 |
| Fighter High (Q15) | 338 | 181 | 59 | 98 | 0 | 28.99% | 8 / 25 |
| Fighter Medium (Q15) | 260 | 141 | 45 | 74 | 0 | 28.46% | 8 / 19 |
| Fighter Low (Q15) | 143 | 81 | 24 | 38 | 0 | 26.57% | 8 / 10 |

No single-blend records or unequal-pair endpoint weights. Robot
local generated C SHA256
`df7ef0c17187c88b6bea96f1378a57b30f942a0433ec682fb93434cb62debca5`;
tracked snapshot SHA256
`de729caf2590729cc9e389c4c678ea50263c6ba07eb610a818212011879b6305`.
Do not equate snapshots or assume cached generated source is current.
The fighter LODs were freshly generated. Pair share is eligible vertex
coverage, not time share or demonstrated performance. No remapping
justified yet.

**Disposition:** V2 passes exactness and is promising enough for consumer
qualification, not default activation. Use matched public plain-deform
control vs selective build on Robot Police with B1/B2 disabled, fixed
simulation, separate phase timing, full final arrays, visual checks,
generated asset hashes, panic and local source evidence. Add unequal
pairs for Q10/Q12/Q0 and representative mixed distributions to target
coverage; the Robot is Q10. Follow with Fighter and relevant placed/
projected consumers if justified. No physical or explicit IRQ proof.
