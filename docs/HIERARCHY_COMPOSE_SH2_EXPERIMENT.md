# B2: exact Q16 hierarchy composition experiment

The current skeleton builds affine T*R*S locals from Q15 quaternions and
Q16 translations/scales, caches unchanged locals, and uses the generic
transform hierarchy's parent-first dirty traversal. Production calls the
already affine-specialized sat_mat4_multiply. Do not substitute the old Q12
MAC.W experiment or sum products before truncation: floor each product
independently. The native leaf schedules DMULS.L and XTRCT, keeps parent
row coefficients in r8-r10, writes disjoint output and preserves the
bottom row. Checked facade falls back to production for aliases and
nonaffine matrices. No divider, no SR.S changes, 12-byte saved-register
stack. No production activation.

Extract the existing evaluator verbatim into a shared templated traversal
with injected composition. The default public function still calls
sat_mat4_multiply; the benchmark compares it against both an unrolled
C++ candidate and a native leaf through checked facades. This avoids
duplicating parent validation or dirty behavior. Matrix creation and
oracle are outside timed intervals; timing includes the entire hierarchy
evaluation, not full pose sampling/skin matrices.

64 rounds per scenario per CPU lane, serial Master then parallel
Master/Slave. 1/22/66/96 joints; branching, long chain, multiple roots,
reverse order, nonuniform/negative scale, unchanged cache and dirty leaf.
Separate edge checks exercise aliasing, nonaffine fallback, invalid
parent/cycle and inadequate scratch. Guard output and compare every
world matrix, dirty/eval state and return code against control; also
independent floor-division software oracle. Each task bounded to one
scenario. Fixed seeds and rotated order. All timing remains diagnostic.

Run `python3 harness/scripts/run_hierarchy_compose.py` and return
`harness/build/hierarchy_compose_bench-return.zip`. Both emulators run
unattended. Target compilation and captures are pending. The Ymir screen
is a diagnostic HUD, not a character visual qualification. Character
scene A/B and native Mednafen visual captures are the next gate only if
exactness and useful speedup hold. Existing B1 experimental projection
must remain disabled for this comparison.

## V1 target results — 2026-11-17

Source checkout `4a85d08d54be22ccc25f54936573ec614f63090b`.
Uploaded combined archive `hierarchy_compose_bench-return.zip` and
standalone Mednafen archive. Every standalone entry is byte-identical
to its counterpart in the combined archive, not an independent run.

Both 1088-byte big-endian reports were independently decoded and checked
against every integer field of the analysis JSON. Both emulators:
all three lanes completed, init/parallel status 0, slave-ready 1, CCR 1,
frequency 210540 Hz. Across six lane executions: 3840 scenario evaluations,
223104 node world-matrix comparisons and 167808 non-root compositions
per timed implementation; 1170 additional edge-check executions.
Zero recorded errors. The counts include repeated executions, not unique
vectors. Ymir ran 4800 fields, cache on, zero misaligned accesses on
both CPUs, immediate-result divider model (the new leaf uses no divider).
Both Python schema and target tests passed. Mednafen 1.32.1, native CD,
data-only cache, exit 0, WRAM swapped16 normalization verified against
ELF text, ELF SHA256
`e213e8e44fde757fc144537d089f70ffbc450d8fed608807a067ac28e0d6f626`.
Ymir does not retain a matching ELF hash. Physical hardware pending.

### Matched hierarchy evaluation, serial Master inclusive FRT sums

| Scenario | Ymir control / C++ / ASM ticks | Mednafen control / C++ / ASM ticks | ASM reduction Ymir / Mednafen |
|---|---:|---:|---:|
| root | 265 / 269 / 271 | 293 / 315 / 325 | -2.26% / -10.92% |
| branch_22 | 15159 / 11388 / 10610 | 15181 / 10945 / 10527 | 30.01% / 30.66% |
| chain_66 | 46448 / 34753 / 32338 | 47365 / 33673 / 32443 | 30.38% / 31.50% |
| chain_96 | 67768 / 50670 / 47127 | 69078 / 49041 / 47250 | 30.46% / 31.60% |
| scaled_branch_66 | 46375 / 34685 / 32291 | 46804 / 33269 / 31991 | 30.37% / 31.65% |
| forest_66 | 43840 / 33030 / 30806 | 44312 / 31808 / 30628 | 29.73% / 30.88% |
| reverse_66 | 45596 / 33877 / 31483 | 46717 / 33033 / 31795 | 30.95% / 31.94% |
| scaled_pose_66 | 46389 / 34672 / 32285 | 46801 / 33259 / 31982 | 30.40% / 31.66% |
| unchanged_66 | 54 / 53 / 53 | 78 / 86 / 86 | 1.85% / -10.26% |
| dirty_leaf_66 | 5712 / 5539 / 5507 | 6264 / 6143 / 6124 | 3.59% / 2.23% |

Positive means reduced elapsed time. Across all seven substantial
composition scenarios and three lanes, ASM beats the reorganized C++
by 6.70–7.09% in Ymir and 3.50–4.16% in Mednafen. C++ accounts
for much of the improvement over production. Both candidates share
the templated traversal; the public comparator calls the original
matrix API, so the overall reduction also includes compilation,
dispatch and direct output effects, not isolated instruction gains.
The root and no-op cases contain no multiplication; small absolute
tick differences can produce large percentages (including tiny
regressions). Dirty-leaf evaluation is mostly traversal.

### Disposition and next gate

Pass the synthetic differential gate. Keep both experimental paths,
do not enable default dispatch, and retain B1 disabled in B2 A/B.
Compare three matched builds on an actual animated character using the
same public API boundary and compiler settings. Measure local pose
construction/sampling, hierarchy evaluation, skin matrices and total
frame work separately. Include dirty and unchanged poses, root-only
cases, actual matrix equality and exact matched simulation-tick
command streams. The benchmark measures preconstructed local matrices,
not full skeleton evaluation or complete rendering. Use the established
lossless screenshot evidence from B1 in both emulators; for animation,
capture matched simulation poses rather than comparing arbitrary video
frame indices. Preserve clean build provenance. ABI/IRQ and real
hardware remain open. The modest incremental ASM benefit must justify
its maintenance cost relative to the portable C++ candidate.
