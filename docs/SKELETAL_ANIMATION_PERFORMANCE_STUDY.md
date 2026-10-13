# Skeletal animation performance study

## Conclusion

Exact skin-record sharing and dual-CPU deformation are now integrated into
the production exporter and example. The robot retains 1,799 faces and 204,568
texture bytes while reducing 1,652 deformation records to 933 (210 rigid and
723 blended). Independent Ymir counters measured **10.0 submitted lists/s**
for the dual-CPU study, versus 7.5 for the original export. The integrated
production build's last CPU sample was 89 ms, including 10 ms deformation.
This is not a promise of presentation FPS on physical hardware.

The experiments live in `harness/studies/skeletal_performance_probe/` and
generated results in `harness/build/`. The study wrapper disables production
parallel deformation so `shared` measures serial deformation and `dual` tests
its isolated split with reference comparisons. Production uses AUTO startup,
serial fallback when Slave is unavailable, and a confirmed runtime shutdown
before recovering from a failed join.

## Implementation and renderer experiments

Sharing happens after skin quantization and compares complete rigid or blend
records, including all joint slots, anchors and weights. The source-to-export
map expands verification back to every original source vertex. Texture baking
still uses original source corners and UVs; only geometric indices are shared.
Original bind bounds are emitted separately so camera fitting does not change.
All 1,805 skin/index/texture tables matched the independently validated compact
prototype exactly. Tests cover rigid and blended seams, distinct weights,
source-order evaluation and bounds beyond a retained representative vertex.

Detailed renderer instrumentation uses FRT ticks and is excluded from normal
production builds. The serial reference measured 1,715 projection ticks,
7,754.5 preparation ticks, 1,299 sorting ticks and 2,472 emission ticks
(medians). These correspond roughly to 8, 37, 6 and 12 ms at this configuration.
The preparation/ordering changes were evaluated and rejected:

| Candidate | Relevant median | Decision |
| --- | ---: | --- |
| Existing grouped buckets | 1,299 sorting ticks | Retained |
| Four-pass exact stable radix | 1,675.5 sorting ticks | Slower; rejected |
| Separate depth/safety scans | 7,754.5 preparation ticks | Retained |
| Fused depth/safety scan | 8,078 preparation ticks | Slower; rejected |
| Small-normal exact dot shortcut | 7,851 preparation ticks | No demonstrated gain; rejected |

The radix candidate passed exact-order, stability, skip, empty and capacity
checks before the hardware run. Its pure algorithm is archived in
`rejected_radix.hpp`; it is not used by the renderer. The instrumented serial
reference measured 8.85 submissions/s, while both radix and fused preparation
runs measured 8.57. Instrumentation and small timing changes can move a frame
past a VBlank boundary: compare candidates with the same instrumentation,
rather than claiming a regression from the earlier uninstrumented prototype.

Dual-CPU deformation then reduced its median from 18 to 10 ms. The isolated
study executed 128 Slave tasks, verified three real poses against serial
output with identical integer coordinates, recorded no errors, and counted
60 submissions in exactly 360 emulator fields (10.0/s at nominal 60 Hz).
An 800-field production run without a RAM cartridge recorded Slave ready,
zero serial recoveries, zero panic and zero misaligned accesses. Its last
completed costs were sample 4, skeleton 9, skin 10, submit 45 and flush 19 ms,
with 89 ms in the whole CPU section. Renderer preparation remains the main
bottleneck; the rejected candidates have not been enabled.

A second production run exercised controls for 1,000 fields. Submission 20
occurred at emulator field 132 and submission 80 at field 492, independently
confirming 10.0 lists/s in the integrated build. Pause snapshots at fields 700
and 740 had identical scene pixels; playback then resumed and camera orbit
changed the final view. Slave remained ready with zero serial recoveries,
zero panic and zero misaligned accesses. Mednafen was also launched through
`run-example.ps1` with no RAM cartridge and displayed the animated textured
character with status OK.

## What the FPS number means

The original HUD sampled `sat_frame_count()`, assumed 60 Hz and truncated
to an integer. The current `SUB/S` display samples `sat_time_ms()` and rounds
to one decimal place. Its value is render-loop throughput over a short window. It is not an independently
measured rate of newly presented framebuffer images. Pausing can raise this
number even though the pose stays unchanged. The timer-based display also removes the original hard-coded NTSC assumption.

The study measures three separate things:

1. CPU stage costs and complete loop intervals, through the guest timer.
2. Submitted-list cadence, through VBlank-IN interrupt counts and an
   independent emulator frame index.
3. Existing VDP1 wait/timeout counters. Actual per-list presentation timestamps
   are not currently exposed and remain a measurement task.

To cross-check the guest clock, Ymir captures a frame when the completed
submission counter reaches 20 and again at 80. These markers are read by the
emulator, not generated by the HUD or guest timer. The baseline markers occurred
at emulated fields 172 and 652: 60 submissions in 480 fields, or **7.5 submitted
lists/s at nominal 60 Hz**. The guest independently counted the same 480
VBlanks and measured 7,999 ms, agreeing with 8,000 ms of nominal field time
within the 1 ms timer quantization. The baseline HUD's integer 7 is consistent
with this rate, but should not be reported as a precise presentation FPS.

The compact candidate's markers occurred at fields 143 and 532: 60 submissions
in 389 fields, or **9.25 submitted lists/s**. Its VBlank-IN counter independently
reported the same 389 fields, and its timer reported 6,484 ms versus 6,483.3 ms
of nominal field time. Sharing records therefore improved independently
measured submission throughput by about **23%** on this workload. These values
are neither host wall-clock FPS nor proof of physical Saturn presentation FPS.

Both experiments use NTSC, the same BIOS, a stationary camera, elapsed-time
animation and no RAM expansion. Emulator execution speed on the host is not
used as a Saturn performance measurement. Mednafen screenshots are visual
checks; their HUD values are approximate observations, not calibrated results.

## Measured costs

Median guest milliseconds per completed frame, excluding the first four
render samples:

| Stage | Original records | Shared records |
| --- | ---: | ---: |
| Clip advance/sample | 4 | 4 |
| Skeleton evaluation | 9 | 9 |
| Skin deformation and matrix copy | 32 | 18 |
| Character scene submission | 51 | 45 |
| Scene flush | 19 | 19 |
| Whole measured CPU section | 117 | 97 |
| End-frame submission | 2 | 2 |
| Independent submitted lists/s | 7.50 | 9.25 |

The whole CPU section also includes stage geometry and HUD work. Stage
medians need not add up to the whole-section median. The compact prototype's
earlier full loop intervals alternated around 100 and 117 ms because the next
VBlank wait quantizes cadence. This is why dividing 1,000 by a CPU-stage time
alone does not give a presentation FPS.

Approximately 900 commands are submitted per pose at the default camera,
including stage/HUD work. The command count stays similar after vertex
sharing because no faces were removed. VDP1 draw-wait and timeout counters
were zero in the observed samples. This means the CPU did not wait on a
previous list at the current slow cadence; it does not prove that the GPU
could sustain 30 or 60 new images/s. `late_swaps` only diagnoses the explicit
ON_SUBMIT swap mode and is not sufficient evidence in the default mode.

## Rejected shortcuts

The Master CCR was already `0x01`, indicating the enabled 4 KiB cache mode.
An isolated purge/re-enable experiment did not reduce CPU stage costs.
Moving the working arrays from WRAM-L to WRAM-H likewise did not materially
improve the previous Ymir measurements. Neither is a demonstrated large win.
The cache-mode values and purge sequence follow Sega's
[dual-CPU technical bulletin, section 6.2, PDF page 93](https://antime.kapsi.fi/sega/files/Sattechs.pdf#page=93)
and the existing `src/hal/sh2/cache.cpp` implementation. Cache publication and
invalidation will still matter if the Slave is used.

Hand-written assembly is not the first step. The current skin loop already
uses SH-2 signed long multiplication (`DMULS.L`); its inspected disassembly
does not call a generic 64-bit multiplication helper. A MAC-based kernel may
reduce accumulation overhead, but must preserve rounding, overflow behavior
and the CPU's saturation-mode contract. Measure it after removing duplicated
work, rather than assuming it solves the frame budget.

Reducing textures or upload size is also not the primary current CPU target:
the textures upload once at startup. Reducing triangles again would trade
away the silhouette improvements and should be reserved for deliberate LOD.

## Implementation order

### 1. Move lossless sharing into the exporter

Deduplicate the quantized deformation records after choosing joint influences,
weights and joint-local anchors. Keep rigid records first. Remap every face
corner and emitted render vertex; preserve face/material/texture ordering.
Recompute counts and size reports. Include every consumed per-vertex attribute
in the key if normals or vertex lighting are added later.

The prototype checks that every remapped face corner resolves to exactly the
same rigid/blended record as before, including all weights and anchors.
Therefore its deformation is identical for any common joint matrices. Do not
merge vertices merely because bind positions are close: differently weighted
vertices can separate during animation. Preserve source bounds or compute
union animation bounds so compaction does not alter camera fitting.

Acceptance: exact deformed-corner equality over all clip samples, identical
face/material/texture tables, stable culling and command accounting, no panic
or misaligned accesses, and a repeated reduction in measured CPU cost.

### 2. Reduce face preparation and ordering cost

The scene currently applies projected-area rejection and then a geometric
visibility predicate, copies accepted faces into general-purpose records,
sorts the whole scene and lowers each face to a command. After sharing vertices,
submission still takes about 45 ms and flush about 19 ms.

Measure projection, geometric culling, record writes, sorting and command
emission separately. Investigate a compact record for projected-safe opaque
faces, retaining the existing near-plane/Gouraud/tiled fallback and canonical
emission functions. Avoid repeatedly copying material descriptors when a
validated immutable material index suffices. Keep residency validation before
hardware writes.

Benchmark an exact stable radix ordering against the current 1,024-bucket
ordering. Preserve pass ordering and equal-depth source order. Do not simply
lower the bucket count: depth-order changes can expose overlapping armor
surfaces. Do not remove geometric culling solely on rounded screen coordinates;
the current code explicitly protects cases where quantization changes winding.

Acceptance: equivalent face admission, clipping, material semantics and
capacity handling across front, side, back, near-plane and zoomed views;
visual checks and deterministic ordering tests; timed improvements.

### 3. Evaluate coarse Master/Slave partitions

`sat_skin_deform_range()` already writes disjoint vertex ranges and
`sat_parallel_for()` already provides a bounded two-CPU join. First benchmark
balanced ranges of the compact mesh with output equivalence and cache
ownership checks. Splitting only skinning cannot achieve 30 FPS: even an
ideal 18-to-9 ms skin split leaves roughly 88 ms of measured CPU work.

Then test face preparation on both CPUs using the existing batch adapter.
Split a large character's work, not individual tiny triangles. Keep sorting,
VDP1/VDP2 access and command emission on Master. Account for private face
buffers, cache invalidation, merge copies and bus contention. Shared projected
vertices should not be projected redundantly by both partitions.

The existing [parallel benchmark report](PARALLEL_RUNTIME_BENCHMARKS.md)
proves worker execution but does not establish a speedup for this character.
AUTO currently keeps geometry conservative; enable it for this workload only
after measuring complete frame intervals and failure recovery.

### 4. Reduce skeleton and face work without changing motion

Cache unchanged local poses instead of marking all 102 nodes dirty every
frame. Let existing parent-change propagation recompute dependent world
matrices. Prune only unused leaf joints after computing the complete ancestor
closure; 61 joints are referenced by the skin, but their parents cannot simply
be removed. Evaluate a tested affine matrix fast path for the joint hierarchy.

Adapt the existing triangle-pair merger to the skeletal surface-transfer
pipeline. Pairs must remain convex and sufficiently coplanar through the clip;
protect articulation boundaries and reject bow-ties. Rebake the combined quad
from the original surface. This can reduce face/command work while retaining
geometry, but the number of acceptable pairs and the resulting gain have not
been measured. Never promise a fixed triangle-to-quad reduction ratio.

## Frame-budget milestones

Use **15 new submitted images/s** as the first milestone, with a measured
complete interval p95 of at most 66.7 ms and independent presentation checks.
This requires substantial renderer improvements beyond the proven sharing
change. A 30 Hz milestone requires complete intervals below 33.3 ms; it is
not a justified forecast from current measurements. Check VDP1 draw-end and
swap timing again as CPU work becomes faster.

Record rendered loops/s, animation updates/s, submitted lists/s, newly
presented lists/s, CPU median/p95/max and VDP1 waits separately. Preserve
elapsed-time playback so these rates do not change the intended clip speed.

## Reproduction

From the repository root:

```powershell
.\harness\studies\skeletal_performance_probe\run-study.ps1 -Variant shared -Bios bios/saturn_bios_us.bin
.\harness\studies\skeletal_performance_probe\run-study.ps1 -Variant dual -Bios bios/saturn_bios_us.bin
```

The runner uses the project build script and Ymir harness, creates an isolated
temporary example, and removes that source directory after the run. Production
example files remain unchanged by the runner. The legacy `baseline` variant now
also measures the current serial export; the original 1,652-vertex measurements
are historical archived results. Its JSON analysis contains guest timer samples,
VBlank counts, emulator counter markers, stage medians and panic state. Stock
RAM and linker stack guards remain enforced by the normal build.
