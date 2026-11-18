# C2 — world-space visibility V1

The scene has two world visibility consumers: a compiled chunk path
calling quad_visible_small_body then quad_visible_general, and a general
path calling quad_visible. Screen-space culling remains untouched.
Existing kernels already use signed16 edge proofs, exact split normal
dots below 2^50 and a historical scaled-normal fallback. Do not alter
winding, centre truncation, collapsed corner order, tie-to-hidden,
double-sided rules or the large normal fallback.

Mode 0 is original. Mode 1 conditionally compares the eye to the exact
truncated centre on the only nonzero normal axis, avoiding the full dot
and unrelated centre arithmetic. In the small path this follows the
existing centre range proof; in wide it follows the exact-limit check.
Mode 2 adds an explicitly unrolled high/low decomposition and skips the
low dot if all low residues are zero. All three modes preserve the
original public defaults, including the inline compiled scene path
(which is not yet redirected to a candidate). No new hardware divide or
Assembly kernel. Scope is the defined original arithmetic domain; this
does not fix signed overflow in the old edge/view subtraction for
arbitrary out-of-domain int32 positions.

24 workloads: small/wide, six signed orientations by winding reversal,
collapsed B/D, collinear, signed16 boundary, wide centre, negative
rounding, exact limit and just below, nonplanar, integer and fractional
normal splits, tangent, extreme view offsets and mixed inputs.
64 batches of 32 faces, serial master and parallel master/slave,
Ymir and Mednafen. Each mode receives identical input; call order
rotated. Verify every visibility byte against scalar signed
quotient/remainder oracle (split 2^26 vs original 2^24); compatibility
oracle uses existing scaled-normal helper for historical fallback.
Guard both ends of each result array. No hardware or real renderer
execution yet. If candidate wins, qualify both compiled and general
scene paths with real command bytes and images before changing default.

Run `python3 harness/scripts/run_world_cull.py`; return
`harness/build/world_cull_bench-return.zip`.

## V1 target results — 2026-11-18

Checkout `682cca405abf6dc9fc411b785bfe7e51a7344e4d`.
Both ZIP integrity checks passed; all standalone Mednafen members
match the combined archive byte for byte. Complete local manifest,
empty tracked/staged/unstaged patches, no nonignored untracked files.
Both 2432-byte telemetry reports independently decoded in big-endian
order; every header, lane and scenario word matches JSON.

Six completed lanes, 9216 batches, 294912 faces and 121254 visible
decisions per implementation, zero errors. Init/parallel status zero,
slave ready, CCR 1, SR.S clear on entry. FRT 210600 Hz Ymir and
210540 Hz Mednafen. Ymir completed 12000 fields, target unittest
PASS, zero reported misaligned accesses on both CPUs. Mednafen
1.32.1 native CD PASS, exit 0, swapped16 WRAM normalization,
ELF SHA256
`146f33d61817dde37703359dfc6e4f6a66f81266b83884129d5acadfa8ce0ba3`.
No separate panic capture or independent Ymir ELF identity check.

### Serial Master FRT sums (64 batches of 32 faces)

| Scenario | Ymir control / axis / split | Mednafen control / axis / split | Split reduction Ymir / Mednafen |
|---|---:|---:|---:|
| small_axis_z | 3751 / 3156 / 3152 | 4199 / 3523 / 3524 | 15.97% / 16.08% |
| small_axis_x | 3741 / 3074 / 3074 | 4193 / 3454 / 3461 | 17.83% / 17.46% |
| small_axis_y | 3740 / 3081 / 3078 | 4183 / 3476 / 3479 | 17.70% / 16.83% |
| small_oblique | 3752 / 4070 / 4068 | 4192 / 4467 / 4466 | -8.42% / -6.54% |
| small_reversed | 3750 / 4072 / 4075 | 4190 / 4465 / 4468 | -8.67% / -6.63% |
| collapsed_b | 13618 / 10915 / 10985 | 14191 / 11557 / 11687 | 19.33% / 17.64% |
| collapsed_d | 13643 / 10963 / 11044 | 14203 / 11619 / 11744 | 19.05% / 17.31% |
| collinear | 14356 / 10619 / 10725 | 14819 / 11243 / 11351 | 25.29% / 23.40% |
| signed16_boundary | 3751 / 4072 / 4073 | 4190 / 4471 / 4462 | -8.58% / -6.49% |
| just_wide | 10624 / 10960 / 10619 | 11450 / 11988 / 11572 | 0.05% / -1.07% |
| medium_axis | 10614 / 7841 / 7815 | 11448 / 8694 / 8778 | 26.37% / 23.32% |
| medium_split_zero | 10630 / 10930 / 9750 | 11445 / 11967 / 10721 | 8.28% / 6.33% |
| medium_split_nonzero | 10623 / 10932 / 10602 | 11448 / 11971 / 11554 | 0.20% / -0.93% |
| tangent | 3749 / 4064 / 4073 | 4190 / 4466 / 4466 | -8.64% / -6.59% |
| wide_center | 7279 / 7346 / 7346 | 7928 / 7969 / 7973 | -0.92% / -0.57% |
| negative_center | 3753 / 4066 / 4067 | 4188 / 4474 / 4465 | -8.37% / -6.61% |
| scaled_normal | 19535 / 19549 / 19610 | 20393 / 20496 / 20570 | -0.38% / -0.87% |
| exact_limit | 19398 / 19415 / 19474 | 20263 / 20361 / 20427 | -0.39% / -0.81% |
| below_limit | 10619 / 7899 / 7875 | 11433 / 8754 / 8836 | 25.84% / 22.71% |
| nonplanar | 3745 / 4071 / 4076 | 4197 / 4466 / 4467 | -8.84% / -6.43% |
| mixed | 7225 / 7537 / 7378 | 7839 / 8252 / 8043 | -2.12% / -2.60% |
| extreme_view | 3741 / 4062 / 4066 | 4195 / 4466 / 4463 | -8.69% / -6.39% |
| negative_split_zero | 10626 / 10931 / 9756 | 11441 / 11968 / 10737 | 8.19% / 6.15% |
| odd_center | 3756 / 4070 / 4075 | 4199 / 4466 / 4467 | -8.49% / -6.38% |

Equal-repeat sum of scenario ticks within each lane: axis-only
6.161–6.172% Ymir and 5.309–5.693% Mednafen; combined
7.581–7.607% and 6.639–6.889%. Peak combined reduction
26.375% on Ymir parallel Master (medium_axis), 23.402% on
Mednafen serial Master (collinear). Worst combined regressions
8.848% Ymir and 6.827% Mednafen. The mixed scenario regresses
on both serial Masters. This is not representative gameplay weighting.

**Disposition:** exactness PASS on the tested defined arithmetic
domain. Do not adopt the V1 combined dispatch: its extra branches in
the signed16 common path consistently penalize non-axial faces.
Next compare a wide-only candidate which preserves the complete
original small/general signed16 kernels (including the inlined
compiled scene path) with a separate optimization of the wide
normal dot: preserve scaled fallback and centre rounding, skip the
zero-normal dot, evaluate single-axis and exact zero-residue
specializations only on the wide route. Consider independently
testing a bounded exact signed16 multiply/accumulate kernel with
explicit MACH/MACL/SR preservation and prove any gain before adding
complexity. Profile real mesh distributions (small/wide, axial,
degenerate, screen rejected and double-sided) before allocating
further renderer experiments. No production activation.
Real renderer timings, command/raster equivalence, physical
hardware and interrupt/ABI qualification still pending. The
original out-of-domain signed subtraction overflow behavior is not
covered by this test and must not be treated as a new contract.
