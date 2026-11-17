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

## V2 results — 2026-11-17

Source `03cb71bbac667a59fe34e1e1a2ea11af9ef5f3a4`. Uploaded combined
`static_project_bench-v2-return.zip` and separate Mednafen archive; every
entry of the latter is byte-identical to the corresponding combined entry.

Both emulators: 19 scenarios x 128 rounds x 3 CPU lanes, 14,592 batch
executions and 1,052,928 vertex executions per implementation across the
two captures. Zero errors, exact expected prefix counts, all lanes done,
init/parallel status zero, Slave-ready 1, all CCR=1 and SR.S clear.
Ymir recorded zero misaligned accesses on either CPU and both Python tests
passed. Mednafen returned zero and its 2,144-byte binary report matched
every integer data field in JSON. FRT 210540 Hz in both. Mednafen ELF
SHA-256 `29a8709baed42f074a7fdb502f08e640845d00e122c67d3226bc77a7863a1640`.
Ymir capture does not establish the identical ELF hash.

| Scenario | Ymir V1 / V2 / control ticks | Mednafen V1 / V2 / control ticks | V2 reduction Ymir / Mednafen |
|---|---:|---:|---:|
| empty | 227 / 188 / 143 | 285 / 268 / 191 | -31.47% / -40.31% |
| singleton | 546 / 444 / 396 | 673 / 527 / 464 | -12.12% / -13.58% |
| rotated_32 | 6971 / 6972 / 8140 | 5842 / 5829 / 7071 | 14.35% / 17.56% |
| rotated_256 | 53471 / 53485 / 64163 | 43326 / 43339 / 54913 | 16.64% / 21.08% |
| sparse_64 | 13603 / 13712 / 13641 | 11201 / 11740 / 11629 | -0.52% / -0.95% |
| behind | 7718 / 7604 / 7235 | 6848 / 6725 / 6325 | -5.10% / -6.32% |
| clamp_x | 8589 / 8525 / 8151 | 7835 / 7519 / 7114 | -4.59% / -5.69% |
| clamp_y | 9058 / 8516 / 8077 | 8205 / 7511 / 7030 | -5.44% / -6.84% |
| large_w | 14060 / 8430 / 8073 | 15657 / 7379 / 6969 | -4.42% / -5.88% |
| mixed_640 | 28970 / 30013 / 29650 | 24711 / 25899 / 25494 | -1.22% / -1.59% |
| rotated_8 | 1989 / 2190 / 2147 | 1844 / 2026 / 1948 | -2.00% / -4.00% |
| rotated_16 | 3659 / 4186 / 4148 | 3183 / 3732 / 3661 | -0.92% / -1.94% |
| rotated_31 | 6769 / 7939 / 7891 | 5689 / 6930 / 6857 | -0.61% / -1.06% |
| rotated_64 | 13599 / 13616 / 16144 | 11196 / 11180 / 13899 | 15.66% / 19.56% |
| reject_first | 26964 / 32341 / 31962 | 21980 / 27823 / 27414 | -1.19% / -1.49% |
| reject_middle | 26975 / 29624 / 31977 | 22009 / 24860 / 27423 | 7.36% / 9.35% |
| reject_last | 26879 / 26941 / 31986 | 21841 / 21943 / 27383 | 15.77% / 19.87% |
| alternating | 31717 / 21992 / 21674 | 29367 / 19142 / 18808 | -1.47% / -1.78% |
| late_y | 27161 / 28450 / 32089 | 22209 / 23563 / 27514 | 11.34% / 14.36% |

Serial Master, elapsed inclusive ticks. Both parallel lanes agree in
direction; dense 256 V2 reduction spans 16.63–16.65% Ymir and
21.02–21.10% Mednafen. V2 dramatically bounds the bad retry behavior
but not every regression: first rejection still does redundant work,
and a separate wrapper is costly for very short batches. Retain 32 as
a conservative experiment threshold; do not infer that it is optimal.
No default production activation.

### Renderer A/B experiment

An internal GNU Make-only `SAT_STATIC_PROJECT_EXPERIMENT=1` opt-in
routes eligible `sat_project_vertices` batches to V2. The default build
does not define it. Small and sparse batches bypass the wrapper at the
API boundary, reducing the extra call cost. CMake release dispatch is
unchanged. The internal flag participates in the existing library build
profile hash. The cube ladder has a fixed rotated camera option; match
the same camera and topology in both variants. Existing cube runner
retains images and timing telemetry and checks the linked kernel symbol.
Use `python3 harness/scripts/run_static_projection_renderer.py` for the
first matched native renderer A/B in Ymir and Mednafen. Compare stage
geometry and full work intervals, commands/faces and Ymir crop pixels;
the Mednafen part reads native telemetry, not a screenshot or hardware
cycle counter. Real games, command-byte equality, ABI/IRQ and physical
hardware remain later qualification gates.

### First renderer capture — harness failure, not a qualified comparison

The uploaded renderer archive at checkout `91dbad1d74592ed4557a53971a2c77baa7ebbb27`
contains a successful build and 1200-field Ymir control capture for
projection-864. Its telemetry header reports 592 submissions, 592 samples,
zero errors/timeouts, 866 vertices and 864 faces per actor. The probe
reports zero misaligned accesses. The runner incorrectly rejected the
panic dump because the linked symbol is now 80 bytes (origin diagnostics
were added), while its guard required exactly 44. The original archive
omitted the panic bytes, so a zero panic record cannot be asserted from
that capture. No candidate or Mednafen results were produced.

Use ELF-derived sizes and verify the zero-initialized panic record,
retain panic dumps in the return archive, and repeat the matched test.
The same stale 44-byte guard existed in the matrix and memory-copy
benchmarks and is corrected there as well. No production rendering
change is necessary.
