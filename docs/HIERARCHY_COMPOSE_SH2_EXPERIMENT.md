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
