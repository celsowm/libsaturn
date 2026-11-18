# C2 — world-cull mode 3 renderer qualification

Use the existing textured Sonic OBJ/MTL/PNG fixture from basic_3d_texture
and its same import parameters. This is real static asset geometry,
not the Tekken or Skybridge game, and does not establish animated mesh
incidence. The separate Robot Police animation follow-up remains useful
if workload coverage or timing is inconclusive.

SAT_WORLD_CULL_EXPERIMENT 0 is the default, 1 matched control, 2 the
selected V2-A (kernel mode 3), 3 census-only original control. The
library build key includes this switch. The compiled submission branch
keeps the exact original signed16 small body, redirects only general
fallback to mode 3; the generic branch uses quad_visible<3>.
No changes to screen-only culling, standalone mesh API, culling order,
double-sided behavior, near clipping or painter. No new ASM leaf.
Enable existing instance projection/preparation metrics in all builds;
extra census branches exist only in mode 3, which is not used for
timing comparisons. The global diagnostic census is single-Master
only, not a concurrent/parallel-scene telemetry API.

Four yaw angles 0,45,90,135 at fixed pitch/distance, each twice.
Eight measured frames with original validated material binding, then
eight with the compiled program binding (same camera poses and mesh).
An explicit diagnostic double-sided mask every 17th face is not
original asset metadata; report that incidence separately. All model
materials must compile, and the census requires exactly the intended
compiled/general route in each frame. The census records input,
bypassed, screen rejected, world tests, small body hits, signed16
general hits, wide fallback, initial zero normal, final zero normal,
world rejects, and actual fast/general instance admissions.
It recomputes route classification only in the census build.

Three builds, each on Ymir and native Mednafen timing and visual runs:
nine executions, 144 recorded frames. Record actual VDP1 command
words including END, and live Gouraud tables with zero capture tails
for every measured frame. A fixed RGB shaded swatch is drawn after
the textured model to exercise Gouraud capture; it is not character
shading. All nine command/table buffers must match literally for
the same frame, including addresses; timing and census builds must
produce the same cull/emission counts. Require zero panic/draw
timeouts and lossless final frozen-pose screenshot equality within
each emulator. Intermediate camera poses compare actual commands,
not independent rasters. Require same generated C asset hash.

Per-phase medians omit first pose pair: submit, projection,
preparation, flush, end, submit+flush. Census-only report has no
performance medians. Report wide incidence and separate world/screen
counts, not extrapolated synthetic speedup. Explicitly keep the
near and screen experiments and B1/B2/B3 off. No activation or
hardware/IRQ/FPS claim. Compilation and target execution pending.

Run `python3 harness/scripts/run_world_cull_renderer.py`; return
`harness/build/world_cull_renderer-return.zip`.
