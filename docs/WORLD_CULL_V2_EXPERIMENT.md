# C2 — world visibility V2

V1 passed exactness but regressed small oblique and mixed geometry.
Retain earlier modes and default 0. Compare only these four modes:
- 0: original.
- 3: wide-only V1 combined specialization (axis and zero low normal
  residue). The signed16 small body is literally mode 0, including
  the general signed16 centre/dot logic on the fallback route.
- 4: mode 3 wide path with a separate signed16-proven DMULS.L leaf.
  Register products, explicit ADDC carry chain, restore MACH/MACL.
- 5: mode 3 wide path with a separate signed16-proven MAC.L leaf.
  Materialize two three-word arrays, clear accumulator, perform
  three MAC.L instructions, read full result and restore MACH/MACL.
  SR.S is read, not modified; when saturation is on use the C++
  dot. Compiler T clobber declared for DMULS.L carry operations.
  No promise to preserve T across an ordinary C++ function call.

All MAC/DMULS leaves are noinline so measured complete predicate
includes call, stack, register preservation and array materialization.
The existing signed16 edge proof bounds the normal L1 magnitude
by 2^32-1, so each partial dot fits signed64 even with INT32_MIN
view offsets. No changes to rounding, winding, degenerate ordering,
screen culling, or scaled-normal fallback. Mode 4/5 use native
instructions only under __sh__, with the same C++ expression on host.
Ordinary builds and scene call sites still use 0, including compiled
small-body path. The benchmark measures standalone complete predicate,
not inlined scene throughput. MACH/MACL are restored explicitly within
the asm blocks; independent ABI sentinels, SR.S-on execution and IRQ
stress are still required before release.

28 workloads: V1's same 24 plus exhaustive 64 signed16 box corners
with extreme view offsets, tangent and small view offsets, and
nearly parallel edges with extreme view. Extra quads have exact zero
centroid to keep original signed subtraction defined. Compare every
result byte and sentinels with V1 scalar split-26 oracle. The historical
scaled-normal branch uses the compatibility helper. A tangent to
one of the initial edges is not necessarily tangent to the selected
fallback normal on a degenerate input; oracle resolves that case.
64 batches of 32 faces, 3 lanes per emulator, 10752 batches and
344064 faces per implementation across both emulators. Rotate and
reverse call order; explicit mode-to-buffer slot mapping with static
assertions. Report 3152 bytes, version 2. Retain V1 runner unchanged.

Run `python3 harness/scripts/run_world_cull_v2.py`; return
`harness/build/world_cull_v2_bench-return.zip`. Compilation and
emulator timing pending. No activation. If native dot loses to the
compiler, reject the native leaf; if wide-only wins, profile real
face distributions before qualifying compiled and general scene
paths. No FPS, physical console or interrupt safety claim.

## V2 target qualification — 2026-11-18

Checkout `c1c437e45b6fb9539ea729fedc4b25670944973c`.
Both submitted archives pass ZIP integrity checks; every standalone
Mednafen member matches the combined archive byte for byte. Local
manifest complete, empty tracked/staged/unstaged patches and no
nonignored untracked files. Independently decoded both 3152-byte
big-endian reports and checked all header, lane and scenario fields
against JSON. No compiler warnings in returned Ymir build log.

All six lanes complete, 10752 batches, 344064 input faces per mode,
134886 visible decisions per mode, zero errors (1376256 total
mode evaluations). Parallel status and init zero, slave ready,
CCR 1, SR.S clear at entry. Ymir 15000 fields, unittest PASS,
reported misalignment totals zero on both CPUs. Mednafen native CD
PASS, exit 0, normalized swapped16 WRAM, ELF SHA256
`fdcda9bbf71be367ff5919789ef13091ac18c2ede246748f036e41a39c17338a`.
FRT 210540 Hz Ymir / 210600 Hz Mednafen. No separate panic dump,
independent target ABI sentinel, saturation-on or IRQ stress test.

### Serial master FRT sums, modes 0 / 3 / 4 / 5

| Scenario | Ymir | Mednafen |
|---|---:|---:|
| small_axis_z | 3743 / 3743 / 4225 / 4367 | 4201 / 4192 / 4772 / 4910 |
| small_axis_x | 3736 / 3744 / 4224 / 4363 | 4197 / 4199 / 4764 / 4913 |
| small_axis_y | 3750 / 3742 / 4220 / 4362 | 4198 / 4196 / 4764 / 4907 |
| small_oblique | 3756 / 3742 / 4222 / 4367 | 4197 / 4201 / 4764 / 4920 |
| small_reversed | 3739 / 3742 / 4210 / 4364 | 4196 / 4200 / 4758 / 4917 |
| collapsed_b | 13624 / 10955 / 11088 / 11099 | 14183 / 11663 / 11862 / 11846 |
| collapsed_d | 13638 / 11013 / 11153 / 11177 | 14211 / 11726 / 11910 / 11903 |
| collinear | 14354 / 10703 / 10855 / 10850 | 14842 / 11311 / 11496 / 11491 |
| signed16_boundary | 3754 / 3749 / 4220 / 4367 | 4195 / 4196 / 4773 / 4909 |
| just_wide | 10623 / 10599 / 10676 / 10678 | 11459 / 11541 / 11662 / 11672 |
| medium_axis | 10619 / 7808 / 7881 / 7898 | 11455 / 8768 / 8887 / 8890 |
| medium_split_zero | 10620 / 9735 / 9814 / 9824 | 11458 / 10691 / 10801 / 10816 |
| medium_split_nonzero | 10617 / 10572 / 10661 / 10669 | 11456 / 11534 / 11648 / 11655 |
| tangent | 3745 / 3742 / 4230 / 4368 | 4209 / 4199 / 4778 / 4911 |
| wide_center | 7273 / 7271 / 7752 / 7900 | 7928 / 7933 / 8581 / 8715 |
| negative_center | 3738 / 3745 / 4224 / 4362 | 4191 / 4200 / 4757 / 4909 |
| scaled_normal | 19526 / 19606 / 19678 / 19690 | 20408 / 20548 / 20681 / 20672 |
| exact_limit | 19394 / 19463 / 19535 / 19536 | 20276 / 20424 / 20551 / 20538 |
| below_limit | 10608 / 7861 / 7941 / 7956 | 11457 / 8824 / 8941 / 8958 |
| nonplanar | 3748 / 3745 / 4226 / 4369 | 4201 / 4189 / 4775 / 4901 |
| mixed | 7218 / 7186 / 7473 / 7547 | 7858 / 7887 / 8216 / 8301 |
| extreme_view | 3742 / 3738 / 4213 / 4358 | 4199 / 4195 / 4769 / 4896 |
| negative_split_zero | 10623 / 9729 / 9811 / 9812 | 11459 / 10691 / 10813 / 10815 |
| odd_center | 3745 / 3748 / 4226 / 4368 | 4197 / 4196 / 4773 / 4911 |
| edge_box_extreme | 5391 / 4831 / 5253 / 5374 | 5854 / 5304 / 5825 / 5943 |
| edge_box_tangent | 5396 / 4830 / 5273 / 5396 | 5850 / 5317 / 5833 / 5961 |
| edge_box_near_zero | 5399 / 4831 / 5257 / 5385 | 5860 / 5311 / 5826 / 5939 |
| near_parallel_extreme | 3738 / 3742 / 4213 / 4362 | 4194 / 4200 / 4765 / 4905 |

Equal-repeat sum-of-ticks reductions, all three lanes:
mode 3 Ymir 8.103–8.161%, Mednafen 6.911–7.002%;
mode 4 Ymir 4.140–4.163%, Mednafen 2.065–2.388%;
mode 5 Ymir 3.042–3.081%, Mednafen 0.961–1.424%.
Mode 3 peak gain 26.553% Ymir (parallel master, medium_axis),
23.791% Mednafen (serial master, collinear). Worst regression
0.594% Ymir and 0.730% Mednafen. Small-oblique serial results
+0.373% / -0.095%; mixed +0.443% / -0.369%.
Assembly candidates regress in the hot signed16 workloads:
serial small_oblique DMULS -12.407%/-13.509%, MAC -16.267%/-17.227%;
worst across lanes DMULS -13.062%/-14.428% and MAC
-16.783%/-17.978%. Their positive aggregate results come from
the shared wide optimization, not an improvement over mode 3.

**Disposition:** retain mode 3 as the sole renderer qualification
candidate. Reject modes 4 and 5 for adoption in their measured
noinline form. Do not infer that all possible inlined/integrated
assembler kernels are slower. Next measure actual incidence of
wide fallback, degeneracy, signed16, screen rejection and double-sided
faces in representative meshes, separately from timing if
instrumentation biases costs; qualify both compiled and general
world-cull consumers with matched VDP1 command and Gouraud captures
and raster equivalence. Do not use equal-weight synthetic sums as
gameplay predictions. Default remains original. Target hardware,
full ABI/IRQ tests and combined C2 qualification remain pending.
