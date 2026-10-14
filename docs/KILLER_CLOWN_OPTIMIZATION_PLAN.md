# Killer Clown performance and presentation plan

Status: profiling, exact lighting arithmetic and paired geometry implemented,
2026-10-14. This document tracks the implementation sequence; remaining
performance, visual and hardware acceptance targets are still open.
It extends the remaining performance work in
[the skeletal animation plan](SKELETAL_ANIMATION_PLAN.md).

## Baseline and objectives

The latest 700-field Ymir smoke reports 63 submitted loops, 657 simulation
ticks and a final short-window rate of 5.4 submissions/s. This rate is neither
a sustained benchmark nor a measurement of unique displayed images.
There are zero panics, body overlaps, draw errors, audio queue failures and
VDP1 timeouts; six late swaps retain the previous image. See
`harness/build/clown_default_analysis.json`. The final list contains 487
textured Gouraud commands, 30 cutout fence commands, 16 additive beam
commands and two floor pools. Counts vary with pose and camera.

Each fighter has 600 faces and 302 deformation records. Textures and RGB
CLUTs are shared; pose, deformation and lighting outputs are independent.
The floor is RBG0 and the crowd is NBG0. Stock-RAM disc boot uses the existing
CMOK boot path. Physical Saturn/Satiator acceptance remains open.

The primary target is a sustained 30 new fight images/s on NTSC, with a
33.3 ms presentation budget, responsive controls, unchanged 60 Hz combat
semantics and readable textured fighters. A 25-image/s PAL mode is an
initial acceptance target. These are goals, not forecasts. A 60-image/s
experiment follows only after the 30-image/s mode passes.

## 1. Measure the complete frame before choosing the bottleneck

Add bounded, preallocated timing records for input/simulation, pose sampling
and skeleton evaluation, skin dispatch and join, bounds, normal accumulation,
normal normalization, spotlight evaluation, camera/fence/VDP2 updates,
geometry preparation, painter sorting, command emission/upload, VBlank wait
and VDP1 completion. Use existing calibrated timing APIs. Account for raw
16-bit FRT wrap: long stages need an extended timer or bounded subintervals.
Do not sum overlapping Master/Slave intervals as elapsed frame time.

Record frame identifiers, submitted and presented list identifiers where
observable, command counts, visible faces, VDP1 completion/late-swap counters,
worker waits and memory high-water marks. A hardware framebuffer/list swap
counter measures presentation; animated screenshot sequences corroborate it.
Identical paused images must not count as a failure to present a list.
Keep simulation frequency and submission throughput separately labelled.

Benchmark fixed idle, walk, attack/jump and close-contact workloads with the
same camera/input and a steady ten-second interval after warm-up. Report
median, p95 and maximum frame times, throughput and missed deadlines. Compare
full lighting, ambient only, body lighting only and beam/pool only. The ambient-only fast path now bypasses mesh normals and spotlight
evaluation; isolated body-only and beam/pool-only measurements remain open.

Acceptance: reproducible stage costs and paired baseline/variant reports.
Instrumentation overhead must be measured, with a normal-build comparison.

Initial delivery: the extended-timer ring and fixed idle/walk/combat runner
are implemented. All three NTSC profiles pass; the matched no-record run
completes the same total number of loops. Lighting is the largest stage at
79–81 ms, followed by 38–39 ms scene preparation. See the
[measured baseline](KILLER_CLOWN_PERFORMANCE.md). Unique image-presentation
tracking, separate normalization/spotlight timings and isolated lighting
variants remain open; stage 1 is a measured baseline rather than full hardware
acceptance.

## 2. Remove redundant work and optimize lighting

Implement dirty tracking for sampled pose, world transform, camera and light
state. Reuse normals when the deformed mesh has not changed. Reuse projection
only when both mesh/world and camera are unchanged. In ambient-only mode,
fill constant Gouraud corrections without normal accumulation, normalization
or cone evaluation. Paused scenes reuse scene preparation where safe while
continuing input, audio servicing and correct framebuffer ownership.

Precompute each light's bounded coefficients once per update; share transformed
vertex positions and intermediate terms across both lights and telemetry.
Separate diagnostic visibility calculations from production lighting, while
keeping an instrumented validation build. Prefer exact algebraic reductions
first. Any reciprocal/square-root approximation needs a documented numerical
error bound and a rendered comparison against the existing implementation.

Evaluate offline rest-pose normals transformed by the skin's joint weights
as an optional faster path. This can differ from normals recomputed from the
deformed faces, especially at bends; retain the reference path and reject
visible seams or incorrect colored highlights. Do not cache by the 20 Hz
source sample index while interpolated poses continue changing.

Acceptance: matched lighting/body coverage, stable gradients during movement,
lower measured CPU cost and no new allocations or per-frame texture uploads.

First arithmetic delivery: exact reciprocal/correction normalization and
shared visibility work reduce lighting medians by 8.36–9.04%, with full-light
throughput improving from 5.67–5.74 to 5.97–6.04 submissions/s. Ambient-only
lighting now bypasses normals/spot evaluation and takes 0.128 ms median.
The failed variable-shift face-normal experiment was removed after measurement.
Dirty-state reuse, finer normalization/spotlight profiling and optional
transformed rest normals remain open. See the
[follow-up measurements](KILLER_CLOWN_PERFORMANCE.md#exact-lighting-arithmetic-follow-up).

## 3. Use the existing dual-SH-2 geometry adapter

The Clown currently submits actor instances serially. Port the existing paired
preparation pattern from the character examples, whose historical HIGH workload
improved from approximately 15 to 20 submissions/s. Those figures are not a
prediction for this model. Register the existing adapter, prepare immutable
material/Gouraud inputs before dispatch, give each actor separate scratch and
merge worker output in source order before one Master sort/emission pass.

Budget the companion face/key buffer in WRAM-L before adding it: WRAM-H is
already tight. Document alignment, cache publication/consumption, buffer
lifetimes and stack headroom. Never dispatch new work onto the Slave while a
skin job still owns its buffers. Keep the Master-only fallback.

Acceptance: serial/parallel equivalence of admitted faces, keys, projected
coordinates and Gouraud values; improved end-to-end time in matched runs;
no cache, ownership or timeout regressions on Ymir and Mednafen.

Paired preparation delivery: the example uses the existing adapter with
separate WRAM-L face/key storage, immutable Gouraud inputs, completed skin
ownership and the original merge order. Matched geometry medians fall from
37.9–38.5 to 26.5–26.6 ms, and throughput rises from 5.97–6.04 to
6.41–6.49 submissions/s. The optional verification build compares every pair
with serial preparation. See the
[paired preparation report](KILLER_CLOWN_PERFORMANCE.md#paired-geometry-preparation-follow-up).
Physical hardware/cache acceptance remains open.

## 4. Reduce geometry and texture fragmentation without losing identity

If profiling still shows preparation/emission dominating, generate deliberate
HIGH/MEDIUM/LOW variants. Initial face-budget experiments are 600/400/250 per
fighter, expressed as named configuration constants. These are trial budgets;
the chosen defaults depend on measured speed and visual acceptance.

Preserve face, hands, shoes and silhouette; validate animation and UV seams
at elbows/knees. Revisit texture-patch tessellation and the 585 RGB CLUTs:
coherent UV quads and compatible patch reuse can reduce command and material
cost, but must retain masked texels and RGB CLUT Gouraud support. Do not merge
polygons across material/UV discontinuities or introduce concave VDP1 quads.
Measure command count, texture/CLUT bytes and artifacts for each export.

Use screen-size LOD with hysteresis only after each variant is accepted.
Combat bodies and hit timing remain independent of visual LOD. A deliberate
quality setting provides reproducible benchmarks. Avoid changing detail every
frame just to hide missed deadlines.

## 5. Improve lights, camera and arena readability

Keep Gouraud on fighter textures as the primary indication of illumination.
Tune light direction, ambient levels and camera framing using matched captures
of idle, punches, kicks and jumps. Keep full bodies and health bars visible;
test the closest camera at opposite cage corners. Improve mat contrast, fence
wire thickness and costume separation without replacing textures with flat
colors. Compare the new variants at native 320 x 224 resolution.

Measure beam overdraw separately. Trial fewer cone sections, tighter bounds
and dimmer unobtrusive fog while preserving the source/aim/floor intersection.
Do not reintroduce mesh on light surfaces. The optional fence mesh is separate.

Current VDP1 light sprites do not add to one another: the last indexed pixel
adds to the VDP2 background. If visible overlap warrants more work, prototype
one combined VDP2 lightmap with both lights accumulated before composition.
First budget a free layer, VRAM bank cycles, update bandwidth, priorities and
fighter exclusion. Do not assume an additional layer can automatically blend
with an already composed RBG0-plus-sprite image. Check composition against the
local SEGA VDP2 manuals and keep the present implementation as fallback.

Use `colored_light_cubes` to isolate math/composition changes before the arena;
it already provides the requested simple two-cube example. Its checkerboard
is a screen-space background, not a perspective floor or a performance proxy
for two skinned actors. A combined-lightmap experiment needs its own clear
overlap comparison in that example.

## 6. Close the original acceptance work

Run builds sequentially with `build-example.ps1`. Run the focused cubes gate,
the 700-field fight smoke, the full menus/two-pad/fence/pause fight gate and
relevant host tests after the corresponding changes. Use Mednafen disc boot
by default and Ymir for automatic RAM/register/image analysis. Compare the
same steady workloads on NTSC and PAL. Preserve CMOK, no expansion cartridge,
cutout ordering, collisions, audio events and complete-frame submission.

Add fast response checks: simulation catch-up currently reads one pad snapshot
per rendered loop, so a nominal 60 Hz simulation does not prove 60 Hz input
sampling. Measure press-to-action and action-to-displayed-image latency before
considering a VBlank input latch or fixed-tick event buffer. Keep interrupt
work bounded and retain transient presses without duplicating attacks.

Before audio changes or regression work, read the SCSP streaming guide.
Validate existing 68000 event execution and queue invariants throughout the
optimization. Review pending scheduled-key cancellation/voice stealing as a
separate correctness task, not as an assumed graphics bottleneck.

Publish a final report with stage timings, sustained presented/submitted rates,
memory budgets, chosen LOD and before/after captures. Physical Saturn/Satiator
and higher-detail targets stay explicitly open until actually tested. Grabs,
throws and intersecting textured surfaces from the original readiness plan
follow the stable performance/presentation baseline.

## Delivery order

1. Profiling and reproducible baseline.
2. Exact lighting reductions and dirty-state reuse.
3. Paired geometry preparation with equivalence checks.
4. Asset/LOD changes only as justified by the new profile.
5. Presentation tuning and optional combined-lightmap prototype.
6. Sustained NTSC/PAL acceptance and original-plan readiness report.

After each step, retain a change only when matched measurements or a specific
correctness/visual improvement justify it. Reprofile before selecting the next
optimization; do not accumulate speculative library APIs.
