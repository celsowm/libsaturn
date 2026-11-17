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
