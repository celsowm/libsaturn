# Static Q16 projection — experimental candidate

Production remains unchanged. The experiment targets the generic camera batch
in rendering/logic.hpp, NOT the Q15 character kernels. Every product is floored
individually before the row sum. The assembly uses DMULS.L / XTRCT, overlaps X
division with Y arithmetic, and Y division with the next vertex's X/W rows.
It accepts a conservatively bounded prefix. Scalar fallback handles every
refused vertex; behind-plane X/Y stay untouched. It drains a pending divide
before returning. Stack and register contracts are in the assembly header.

The diagnostic compares the complete candidate wrapper, including fallback,
against the existing production project_vertices batch and against a separate
software quotient oracle. It verifies whole guarded output arrays, alternating
timed order, reports prefix coverage and runs 128 batches per scenario on each
of three CPU lanes. Scenarios: empty, singleton, general 32 and 256, sparse,
behind, clamp X and Y, large W and mixed 640x448. No approximate arithmetic,
new public API, or release-path selection is introduced.

Run both emulator captures: python3 harness/scripts/run_static_projection.py
(return the generated static_project_bench-return.zip). Timing is inclusive
FRT, with no assertion that the candidate is faster. Decide whether to
integrate only after matched workload measurements and hardware-aware review.
The Ymir immediate divider model cannot establish hardware divider latency.

## V1 results — 2026-11-17

Source checkout: `f582ef02a83ba1acccebf4a97de1d08099406088`. Two uploaded
archives: `static_project_bench-return.zip` and
`static_project_bench-mednafen-return.zip`. The standalone Mednafen analysis
is byte-identical to the one in the combined archive: it is not a third run.

- Ymir: cache enabled, 2400 fields, immediate-result-no-busy-wait division
  model, FRT 210540 Hz, zero misaligned accesses on both CPUs.
- Mednafen 1.32.1: native CD boot, 90-second unattended capture, exit 0,
  data-only cache emulation, FRT 210600 Hz, ELF
  `0a9255a789df3273af91124a26432597a3a4d9a870bbc94964ed43b40e65f74a`.
- Both: all three lanes complete, init/parallel status 0, Slave-ready 1,
  CCR 1 and SR.S clear. Each lane tested 1280 batches and 78336 vertices.
  Total 7680 batch executions and 470016 vertex executions **per implementation**
  across both emulators. No mismatches against either the optimized control
  or the independent oracle. These are executed cases, not unique vectors.

### Serial Master inclusive FRT sums

| Scenario | Ymir candidate/control | Ymir reduction | Mednafen candidate/control | Mednafen reduction | Fast vertices / total per lane |
|---|---:|---:|---:|---:|---:|
| empty | 226/148 | -52.70% | 280/219 | -27.85% | 0/0 |
| singleton | 552/395 | -39.75% | 690/490 | -40.82% | 128/128 |
| rotated_32 | 6979/8150 | 14.37% | 5903/7129 | 17.20% | 4096/4096 |
| rotated_256 | 53511/64190 | 16.64% | 43647/55175 | 20.89% | 32768/32768 |
| sparse_64 | 13623/13658 | 0.26% | 11285/11710 | 3.63% | 8192/8192 |
| behind | 7725/7259 | -6.42% | 6900/6399 | -7.83% | 3328/4224 |
| clamp_x | 8591/8155 | -5.35% | 7893/7170 | -10.08% | 3328/4224 |
| clamp_y | 9067/8080 | -12.22% | 8289/7100 | -16.75% | 3328/4224 |
| large_w | 14063/8095 | -73.72% | 15734/7030 | -123.81% | 0/4224 |
| mixed_640 | 28998/29684 | 2.31% | 24863/25644 | 3.05% | 13824/16256 |

Reduction = (control - candidate) / control. Positive is better. Parallel
Master and Slave agree in direction: rotated_256 reduction about 16.6% Ymir,
20.8% Mednafen. No frame-rate claim follows from these synthetic timings.

### Verdict and V2 gates

Retain the experimental kernel; reject unconditional production dispatch.
The large rotated batches merit further work against the real production
control. Small/sparse batches have little or no useful benefit. V1 repeatedly
attempts a prefix on refusal, recomputing a rejected vertex in the scalar
path and paying repeated setup when coverage is poor. Large W demonstrates
the extreme case: zero fast vertices and a severe regression.

Next, compare guarded dispatch that retains C++ for small and sparse batches,
and switches the remaining suffix to the existing optimized batched control
on the first rejection (rather than restarting Assembly at every vertex).
Compare this against V1 and the unmodified C++ path, including late rejection,
alternating eligible/refused vertices and representative scene geometry.
A threshold is a hypothesis until measured. Preserve exact output, count
zero unexpected mismatches, and require representative dense/general wins
without material regressions. If guarded dispatch is sound, qualify with
actual cube/world/consumer workload A/B captures and matching frame/command
hashes before enabling it. Hardware and ABI/IRQ audit remain open.

## V2 — single-prefix guarded dispatch

The original V1 retry wrapper and assembly are retained unchanged. The
experimental V2 wrapper bypasses batches below 32 vertices and sparse camera
matrices, attempts the assembly exactly once, then gives the whole remaining
suffix to the existing general C++ batch. No scanning, speculative matrix
classification cache, or production change. The threshold is deliberately
a hypothesis until measured. A first-vertex refusal still incurs one setup
and partial projection, so zero regressions cannot be promised in advance.

The V2 capture compares V1, V2, and production in the same executable,
rotating their timed order (all six permutations), and checks all against
the independent oracle over the entire guarded output arrays. Original ten
scenario input streams are preserved. Nine more scenarios test sizes 8,
16, 31, 64, and 128-vertex early, middle, late, alternating and late-Y
rejections; exact fast-prefix counts are asserted. Parallel work is divided
into groups of at most three scenarios per dispatch. The schema is versioned
and the analysis retains separate V1/V2 elapsed FRT counts and coverage.

Run `python3 harness/scripts/run_static_projection.py` from the repository
root. Send `harness/build/static_project_bench-v2-return.zip` (combined
Ymir and Mednafen). The former V1 archive is not overwritten by the V2
combined runner. Compare timings within the same V2 capture rather than
assuming earlier binary/cache layouts match. Review all three CPU lanes;
verify dense savings, threshold overhead, sparse and rejected fallback costs.
If these gates hold, move to a real renderer workload A/B; do not enable
production based solely on synthetic results.
