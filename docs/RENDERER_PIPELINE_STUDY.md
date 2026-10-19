# Renderer pipeline study

Study of the retained `f4ec82b` Dino build, its complete 12-field Ymir trace
at fields 1160–1171, the cube ladder, and the primary hardware manuals.
This records evidence and the next experiments; it changes no runtime policy.

## What the trace establishes

The application produces about one CPU submission per three nominal NTSC
fields. Four sampled completed VDP1 lists take 57,137, 57,182, 56,095 and
56,365 system cycles respectively. The median field span is 449,204 cycles;
these observed lists therefore take a median 0.1263 fields to draw. This is
a narrow observation for this Dino camera and workload, not a bound for other
screen coverage, transparency, resolutions or scenes.

The 12 frame snapshots expose an 11-field endpoint interval: four draw starts,
four draw ends, four framebuffer swaps and three newly visible completed draws.
These are different counters with different boundaries. CPU submissions and
framebuffer swaps must not be advertised as distinct displayed frames. The
report deliberately does not turn this short interval into a presentation FPS.

Exclusive Master function costs in the full 12-field CPU histogram:

| Routine | Cycles | Share |
| --- | ---: | ---: |
| `sat_scene3d_faces_submit_instance` itself | 886,586 | 16.45% |
| General projection kernel | 658,870 | 12.22% |
| VDP1 `push_distorted_sprite` | 411,420 | 7.63% |
| Face `append` | 408,002 | 7.57% |
| `wait_vblank` itself | 371,328 | 6.89% |
| Face flush itself | 311,715 | 5.78% |
| Cache `invalidate_range` | 272,183 | 5.05% |
| `sync_batch_sources` itself | 248,508 | 4.61% |
| `draw_distorted_impl` | 231,307 | 4.29% |
| Painter ordering | 199,857 | 3.71% |
| Material revalidation | 138,739 | 2.57% |
| VDP1 submission itself | 124,038 | 2.30% |
| Prepared-face merge | 121,773 | 2.26% |

These rows exclude time in child functions; do not add inclusive durations to
them. The source traversal and cache invalidation rows together account for
9.66% of this Master trace. Merge is a smaller measured target. The two
distorted-command assembly layers together account for 11.92%, before adding
other drawing paths. Removing a routine does not necessarily recover all of
its execution cycles as frame time: useful work, synchronization and waits
overlap across CPUs.

## Cache configuration was incompletely described

Every snapshot in this capture reports Master CCR `0x01`, Slave CCR `0x00`.
The harness option `--sh2-cache on` enables **emulation** of the caches; it does
not force each CPU's cache-enable bit. `saturn_slave_entry` and
`saturn_slave_init` establish SR, VBR, stack and FRT state without explicitly
enabling cache. This is a confirmed property of these injection captures,
not a measurement of a physical Saturn or a Mednafen register dump. Earlier
before/after comparisons retain their measured configuration, but must not be
described as having both CPU caches enabled.

The new BIOS-mode Ymir trial stopped at the BIOS's initial language setup
screen. Its CCR values therefore do not establish the application's CD-boot
configuration. The earlier independent Mednafen CD boot shows the running
Dino, but did not capture CCR. This question remains open for an actual CD
application run.

Cache enablement alone is not yet a demonstrated frame-rate fix. The Slave
also spends substantial time polling mailbox/control state. First establish
how much of its useful work lies on the Master's critical path. Enabling its
cache requires exercising every task's nested pointers, repeated jobs,
registration updates, stop/restart and mixed cached/uncached aliases.

## Ownership and memory traffic

`submit_model` publishes the Slave batch, prepares the Master partition,
joins the Slave task and merges its prepared records. The next pose decode
is requested before final face flush, overlapping that decode with emission.
The two partitions are already independently projected and have separate
scratch/output storage. Replacing this with an immediate join or moving pose
decode back to the Master would discard existing overlap.

`sync_batch_sources` traverses the nested graph on both CPUs for every
geometry batch. It visits mesh descriptors, dynamic vertices, immutable index
and face-material tables, the full material table, referenced texture/tile
descriptors, optional Gouraud data and scratch arrays. A validated topology
binding skips validation work; it is not an initial publication token or a
cache-coherence contract. Skipping these operations because a binding exists
would be incorrect for a previously cached receiving CPU.

The retained SH-2 ELF reserves 62,400 bytes for 600 Slave output faces: each
record has a 104-byte stride. A projected-safe face does not use its 48-byte
world-coordinate fallback. Append and merge already avoid copying that field
for safe faces, but allocation and full-capacity output invalidation still
span it. Material storage occupies 35,168 bytes for 1,256 entries. These are
target ELF sizes, not host `sizeof` results with 64-bit pointers.

`consume_slot` invalidates the entire output capacity on the Master. The
geometry adapter also synchronizes scratch arrays and keys. Some are written
through uncached worker aliases, and current key synchronization is required
because previous merges may have left Master cache lines populated. Removing
purges needs an explicit ownership transition, not just a smaller face struct.

## Constraints from the manuals

The newly indexed [SH7604 cache chapter](sega_saturn_hardware/manuals/sh7604/12-section-8-cache.md)
describes CCR, write-through operation, cache-through accesses, initialization,
specific-line purges and data coherence. The
[Saturn Dual CPU User's Guide](sega_saturn_hardware/manuals/saturn-dual-cpu/README.md)
provides Sega's system-specific guidance for shared external devices and
receiving data through cache-through reads or invalidated cache lines.
Cache management instructions must respect the execution-address restrictions
in the hardware manual; a CCR change is not an arbitrary C store to insert
wherever convenient.

The [VDP1 VRAM map/access notes](sega_saturn_hardware/hard/vdp1/hon/p02_10.md)
describe CPU/system-controller priority over drawing and resulting waits.
Preparing commands in Work RAM and transferring them after the old list ends
has different contention from patching VRAM while it is being drawn.
The [plot trigger register](sega_saturn_hardware/hard/vdp1/hon/p04_13.md)
starts drawing immediately when written in plot-now mode, and
[EDSR](sega_saturn_hardware/hard/vdp1/hon/p04_16.md) tracks fetching END.
Command-table reuse, drawing end and framebuffer visibility are separate
ownership boundaries. Consult the
[VDP1 supplement](sega_saturn_hardware/manuals/vdp1-supplement/README.md) too.

The [SCU DMA restrictions](sega_saturn_hardware/hard/scu_/hon/p02_10.md)
exclude Work RAM Low from SCU DMA. The Dino's Slave face records live there;
blindly replacing their merge with SCU DMA is not a valid optimization.
On-chip SH-2 DMAC and SCU DMA have different capabilities and constraints.
The [programming manual's pipeline section](sega_saturn_hardware/manuals/sh1-sh2-programming/07-section-7-pipeline-operation.md)
helps evaluate scheduling but does not model Saturn shared-bus contention.

## Next experiments, in order

1. Add bounded phase measurements around Master input publication, worker
   start/preparation/output publication, Master geometry, join, merge,
   revalidation/order/emission and VDP1 transfer. Record actual CCR on both
   CPUs. Compare local CPU intervals independently; do not subtract unsynced
   FRT counters across CPUs. Record useful Slave work separately from polling.
2. Test explicit Slave cache initialization as a controlled configuration,
   independently of representation changes. Verify evolving poses, changing
   camera/material data, controls and repeated task handoffs before retaining
   any result. Check CD boot separately. Keep this even if the outcome is
   "Slave faster, total frame unchanged."
3. Introduce an immutable resource publication lifetime separate from dynamic
   pose/scratch ownership. Publish topology/material/texture descriptor ranges
   once per generation on each receiving CPU; publish dynamic data per job.
   Reject stale generations after edits or texture recycling. Keep flush-time
   residency checks until an explicit replacement contract guarantees them.
4. Measure prepared material/command templates with a bounded command-range
   reservation. Precompute invariant command fields; patch only coordinates
   and frame-dependent Gouraud state. Preserve global painter ordering,
   failure atomicity, clipping fallbacks and previous-list completion.
5. Then measure compact safe-face records or a borrowed prepared-span merge.
   Preserve batch lifetime through flush, release/cancel behavior, capacity
   checks and the existing copying path for independent callers where needed.

Repeat the two-cube controls and all three Dino scenarios after each isolated
change. Require identical fixed-pose images, advancing animation, no failed
jobs/overflow, complete traces and a reduction in total CPU work or measured
critical-path time. Hardware performance claims still require hardware data.

## Reproduce the report

```sh
python3 harness/scripts/renderer_pipeline_study.py \
  --prefix harness/build/dino-project-split-animated/field-1200_profile \
  --elf harness/build/dino-project-split-animated/dino_demo.elf \
  --out harness/build/dino-project-split-animated/renderer-study.json
python3 tests/tools/test_renderer_pipeline_study.py
```

The script reconciles the CPU histogram through the existing profile analyzer,
rejects dropped captures and missing/non-monotonic frame data, deduplicates
repeated list timestamps, and reports actual CCR values and both draw/visible
counters. It never infers a completed draw from a previous list's END timestamp.
The retained normal-BIOS trial is `harness/build/dino-renderer-study-cd`.


## Bounded phase measurements

`DINO_PIPELINE_METRICS=1` now enables a 64-entry completed-submission ring in
Dino and selects the generic library's profiling variant. The normal build
keeps these probes disabled. The worker profile is internal diagnostic state,
written and read through uncached aliases after successful geometry completion;
it assumes the executor's single active geometry callback. No public scene
record layout or cache policy changes.

The Master uses its extended timer for containing intervals. Worker source,
geometry and output intervals use only its local FRT. Both CPUs' actual CCR
and TCR values are retained. The decoder requires both prescalers to be /128,
checks component sums and containing intervals below a 16-bit wrap, and uses
the Master's nominal calibration only to convert durations. It never subtracts
Master and Slave counter origins. Measurements include stalls and interrupts,
not just retired instructions. Instrumentation itself adds some work.

The `dino-phases-v2-animated` retained build produces these median elapsed times
across 63 completed submissions at the 1200-field checkpoint:

| Phase | Median ms |
| --- | ---: |
| Collect previous pose | 0.347 |
| Publish geometry and dispatch | 3.567 |
| Master geometry | 19.844 |
| Join, including completion consumption | 2.342 |
| Executor response waits inside that join | 1.145 |
| Merge prepared faces | 1.149 |
| Flush, including validation, ordering and emission | 13.518 |
| Painter ordering inside flush | 2.114 |
| Emission inside flush | 9.661 |
| End frame, including VDP1 submission/transfer | 1.168 |
| Complete measured work, excluding initial VBlank wait | 44.571 |
| Worker source synchronization | 3.885 |
| Worker geometry | 17.032 |
| Worker output synchronization | 0.123 |
| Complete worker measured interval | 21.041 |

The table contains nested and overlapping intervals: do not sum all rows.
Join is **not** pure waiting for worker computation: it also consumes completion
and invalidates the output. The executor already distinguishes response wait
from that work; the ring now retains its per-join delta. This narrows the
observed worker response wait to about 1.15 ms, while the total Master geometry
and emission stages remain much larger. Making the Slave faster can also
change shared-bus contention, so this is not a general speedup upper bound.

This confirms the next optimization targets: repeated resource publication
and face/command preparation on the Master, rather than assuming that Slave
cache enablement alone will reach 30 or 60 submissions/s. The measured Master
work exceeds the two-field budget even before its next initial VBlank wait.
No renderer speedup is claimed for these diagnostic changes.

The animated and paused scenarios retain 20 nominal CPU submissions/s; controls
retain 18.7. All three pass the existing runtime gates at 600 and 1200 fields.
Master CCR stays `0x01`, Slave CCR stays `0x00`; both FRT prescalers are /128.
The fixed-pose crop matches the previous retained baseline exactly. The new
trace is checked for dropped observations by `dino_pipeline.py` as before.

Reproduce with the toolchain and Python asset dependencies installed:

```sh
make all EXAMPLE=dino_demo DINO_PIPELINE_METRICS=1 -j4
python3 harness/scripts/dino_pipeline.py --bios /path/to/bios.bin \
  --out harness/build/dino-phases-animated --profile
python3 harness/scripts/dino_pipeline.py --bios /path/to/bios.bin \
  --out harness/build/dino-phases-paused --scenario paused
python3 harness/scripts/dino_pipeline.py --bios /path/to/bios.bin \
  --out harness/build/dino-phases-controls --scenario controls
python3 tests/tools/test_dino_phase_metrics.py
```

Each checkpoint retains the raw `*_g_dino_pipeline.bin` and decoded
`*_phases.json` alongside the existing diagnostics. The decoder rejects missing
worker completion, inconsistent phases, possible timer wrap and invalid clock
configuration. When the ring wraps it excludes the next slot, which the stopped
guest may have started overwriting. A fresh output directory is required.

Validation also includes the complete `make test` suite with the asset Python
virtual environment, the normal diagnostic-disabled Dino build, and a 30-second
Mednafen CD recording (`harness/build/dino-phases-v2-mednafen`). The recording
shows the animated textured Dino and HUD after native disc boot. This separately
checks execution without the Ymir application-injection path; it does not dump
Mednafen CCR state or certify physical hardware timings.

## Compiled source publication plans (2026-10-18)

The first optimization after phase measurement removes repeated traversal of
immutable source resources. `sat_scene3d_source_plan_init` compiles the index
array, material table, face-material map, texture runs and nested tiled
texture descriptors into caller-owned memory ranges. Initialization publishes
these resources on Master. An optional `source_plan` on a prepare item then
skips that material/descriptor walk at submission and at reception.

This deliberately keeps receiver invalidation: each execution consumes every
compiled range, whether it runs on Slave or falls back to Master. There is no
per-CPU acknowledgment to become stale after executor shutdown/restart.
The submitting CPU only synchronizes dynamic ranges after initialization.
Vertices, mesh/instance descriptors, bounds, transforms, face masks and scratch
remain per-task data. Gouraud **values** are also dynamic: their ranges follow
`static_count` in the plan and are synchronized on both sides of every task.
The conservative path still walks the graph when `source_plan == NULL`.
Face-mask synchronization is now explicit in both paths.

The plan stores a copy of the validated topology identity and rejects changed
pointers/counts. This check does not detect edits at the same address. After
changing indices, material mappings, materials, texture/tiled descriptors, or
Gouraud addresses, finish and release every borrowing task, rebuild the binding,
and initialize the plan again. Keep the plan and range storage alive and
immutable until those tasks are released. Texture residency and VRAM ownership
checks at flush are unchanged; publication never extends a texture lifetime.

For example, after loading textures and validating an instance binding:

```c
static sat_scene3d_source_plan_t sources;
static sat_scene3d_source_range_t ranges[128];

/* Check the result: complex tiled resources may need more range storage.
 * A failed initialization leaves sources invalid and may write ranges. */
sat_result_t result = sat_scene3d_source_plan_init(
    &sources, &instance, ranges, 128);
if (result != SAT_OK) return result;
item.source_plan = &sources;
```

There is no allocation. Dino reserves `MODEL_TEXTURE_CAP + 3` ranges for its
untiled, non-Gouraud material layout; consecutive texture descriptors coalesce
as before. Other material layouts must provision their own capacity and handle
`SAT_ERR_CAPACITY`. Zero-initialize prepare items before setting optional fields.

The retained instrumented animated runs compare
`harness/build/dino-phases-v2-animated` with
`harness/build/dino-source-plan-animated` at field 1200 (63 completed samples):

| Local elapsed phase | Before median | After median |
| --- | ---: | ---: |
| Master publication/dispatch | 3.567 ms | 0.347 ms |
| Master geometry | 19.844 ms | 19.839 ms |
| Join including completion consumption | 2.342 ms | 1.125 ms |
| Executor response wait within join | 1.145 ms | 0.000 ms |
| Worker source consumption | 3.885 ms | 1.225 ms |
| Worker geometry | 17.032 ms | 17.028 ms |
| Flush | 13.518 ms | 13.523 ms |
| Master work, excluding initial VBlank wait | 44.571 ms | 40.033 ms |

Work decreases by about 10.2%. The worker source improvement comes from removing
the material traversal; its invalidation remains. Worker intervals overlap
Master intervals and must not be added to them. CCR remains Master `0x01`,
Slave `0x00`; cache policy, geometry and command emission are unchanged.
Default animated and paused cadence stays at 20 nominal CPU submissions/s.
The controls scenario improves from 18.7 to 20.0 submissions/s over fields
600–1200. These remain emulator submission measurements, not distinct display
FPS or physical hardware timing. The two-field/30 Hz work budget remains unmet.

All three runs pass the existing runtime gates with no failed tasks, panic,
command overflow or music error. The paused model crop has zero changed pixels
against `dino-phases-v2-paused`. The animated profile has no dropped observations.
A separate 30-second native-disc Mednafen recording is retained in
`harness/build/dino-source-plan-mednafen`; the capture shows the textured Dino
and HUD. Full host/tool tests pass with the asset Python environment.
The scene parallel host test additionally covers capacity/publication failure,
nested tiled resources, changing pose buffers, dynamic Gouraud values, face
masks, repeated receiver consumption, topology mismatch, rebuilding after
same-address descriptor edits, and the conservative NULL-plan path.

Reproduce using the phase-measurement commands above with a fresh output
name for each scenario. Compare the fixed pose with:

```sh
python3 harness/scripts/dino_pipeline.py --compare \
  harness/build/dino-phases-v2-paused harness/build/dino-source-plan-paused
```

The diagnostic-disabled Dino build also passes native image/RAM checks with
no phase-ring or worker-profile symbols. The conservative cube path is checked
separately in `harness/build/cube-source-plan-regression`: scene cubes retain
60 nominal submissions/s and 1.900 ms median work; paired dense cubes retain
15 submissions/s and 58.186 ms median work, with no panic or timeout. These cube
items do not opt into publication plans and establish regression coverage,
not a speedup claim for that path.

## Fused projected sprite emission

The next isolated change removes three intermediate coordinate-array copies
from opaque projected indexed faces. The scene calls an internal graphics
entry, which checks initialization and passes the quad and current texture
descriptor directly to the HAL command writer. All 16 command words are
written into the existing Work RAM list. This is an automatic scene-path
optimization; applications need no new public API or immutable-resource plan.

Descriptor validity, dimensions, sprite effects, world/overlay budget and END
reservation are still checked before advancing command occupancy. Global
painter order, owner/residency preflight and scene rollback remain in place.
RGB polygons, clipped faces, Gouraud and color-calculation sprites retain their
existing dispatch. No cached material template, VRAM list lifetime change or
Slave cache-policy change is introduced in this step.

Against `772d2cb`, the retained `dino-direct-animated` run at 1200 fields has
63 complete samples, unchanged Master/Slave CCR (`0x01`/`0x00`) and /128 FRTs:

| Median interval | Before ms | After ms | Change |
| --- | ---: | ---: | ---: |
| Emission | 9.668 | 6.728 | -30.4% |
| Complete flush | 13.523 | 10.584 | -21.7% |
| Complete work, excluding initial VBlank wait | 40.033 | 37.123 | -7.3% |
| Master geometry | 19.839 | 19.839 | unchanged |
| Worker geometry | 17.028 | 17.028 | unchanged |

These are nested/overlapping elapsed intervals from Ymir, not physical-hardware
measurements. Animated, paused and controls scenarios still report 20 nominal
CPU submissions/s over fields 600..1200, without runtime-gate failures. The
work remains above the approximately 33.3 ms two-field budget; this is not a
30 FPS result. The fixed-pose crop has zero changed pixels against
`dino-source-plan-paused`.

The independent cube regression `cube-direct-regression` retains 60 submissions/s
with 1.899 ms work for scene cubes. Paired 864-face cubes retain 15 submissions/s
while work falls from 58.186 to 54.123 ms (7.0%). Native CD boot was recorded
and visually checked in Mednafen under `dino-direct-mednafen`. The normal Dino
build without diagnostic symbols passes the same 600/1200-field runtime gates
at 20 submissions/s (`dino-direct-normal`).

Host coverage compares every command word against the original distorted
writer over 2048 deterministic texture/flag/coordinate cases. Poisoned command
storage detects omitted fields; capacity, invalid descriptors, unsupported
effects and overlay reservations must leave occupancy unchanged. Scene tests
exercise the fused route through their existing painter/failure spies. The
full local suite and phase-comparison tests pass.

Reproduce builds and all three scenarios using the earlier commands with
fresh `dino-direct-*` output directories. Compare retained raw phase rings and
fixed-pose images without inline analysis scripts:

```sh
python3 harness/scripts/dino_phase_metrics.py --compare \
  harness/build/dino-source-plan-animated harness/build/dino-direct-animated
python3 harness/scripts/dino_pipeline.py --compare \
  harness/build/dino-source-plan-paused harness/build/dino-direct-paused
python3 harness/scripts/cube_pipeline.py --cases scene paired-864 \
  --bios /path/to/bios.bin --out harness/build/cube-direct-regression
python3 harness/scripts/cube_pipeline.py \
  --record-cd harness/build/dino-direct-animated --record-example dino_demo \
  --bios /path/to/bios.bin --out harness/build/dino-direct-mednafen --seconds 30
```

The phase comparison rejects mismatched scenario, BIOS, emulator binary,
checkpoint windows, FRT calibration or observed cache policy and decodes the
raw rings through the same consistency checks as capture. A percentage is
undefined (`null`) when the baseline median is zero. The largest remaining
measured interval is Master geometry; evaluate its shared-memory traffic and
the face-record layout before choosing the next implementation.

## Face layout experiments and bounded admission kernels

The 104-byte face record was measured before adopting an ABI refactor. An
experimental tagged union overlaid the 16-byte projected quad and 48-byte world
quad, reducing each 32-bit target record to 88 bytes. Admission and merge copied
only the active union member. The complete local suite passed and the paused
Dino crop was identical, but animated work increased from 37.123 to 37.179 ms;
Master geometry increased from 19.839 to 19.967 ms. Capacity invalidation fell
with the allocation size, while join fell from 1.130 to 0.973 ms and merge rose
from 1.144 to 1.206 ms. Smaller storage did not establish faster rendering.
The union change was removed. Retained private evidence is `dino-union-*`, its
phase comparison and `harness/build/face-union-experiment.patch`.

A second experiment replaced four-corner 64-bit means with exact quotient and
remainder arithmetic in 32 bits. Boundary combinations and 100,000 random
means agreed with the wide oracle, but the resulting geometry was slower on
both CPUs. It was removed as well (`dino-mean-*`, `mean4-experiment.patch`). Its
FRT calibration was 210540 rather than 210600 Hz, so the strict comparison tool
correctly rejects it as a matched-clock comparison. This experiment is not
used to quantify a speedup.

The retained change specializes face admission at compile time after the
existing instance-wide min-W and coordinate proof. The proven kernel needs
neither a per-corner positive-depth predicate nor world-clipping branches or
world-corner arguments. Positive near depth is required by scene begin; only
the internal proof selects this kernel. Painter depth keeps its full-width
sum, truncation, pass bits and saturation; capacity and shading are still
processed. Faces without the proof enter the general kernel.

The exact world visibility predicate is emitted as a separate callable kernel
instead of being expanded into each scene loop. In the measured SH-2 image,
`submit_instance` shrinks from 5488 bytes with specialized admission to 4656
bytes; the visibility kernel occupies 508 bytes. This is a code/register
allocation experiment on a CPU with a 4 KiB cache, not proof of a particular
cache-hit improvement. The isolated admission run (`dino-append-animated`)
has 36.823 ms work; adding the separate visibility kernel gives 36.719 ms.

Against the `0b1f698` baseline, the final `dino-cull-animated` ring has 63 complete
samples at fields 1007..1193, matching 210600 Hz calibration and observed
Master/Slave CCR 1/0:

| Median interval | Before ms | After ms |
| --- | ---: | ---: |
| Master geometry | 19.839 | 19.435 |
| Worker geometry | 17.028 | 16.804 |
| Complete work | 37.123 | 36.719 |

Complete work improves by 1.1%; animated, paused and controls retain 20 nominal
submissions/s. This remains above the two-field budget. The paused model crop
has zero changed pixels. Scene cubes retain 60 submissions/s and 1.899 ms work;
paired 864-face cubes retain 15 submissions/s while work falls from 54.123 to
53.300 ms (`cube-cull-regression`). These are Ymir observations, not certified
hardware or distinct presentation FPS.

The admission oracle now includes the exact near plane, positive depth sums
beyond int32 and saturated painter keys. Existing randomized exact visibility,
Gouraud, fallback, rollback, parallel and owner tests run through the retained
kernels. The complete host/tool suite and installed-package CTests pass. A
native-disc Mednafen recording is retained in `dino-cull-mednafen`.

Reproduce the earlier build/capture commands with `dino-cull-*` and compare:

```sh
python3 harness/scripts/dino_phase_metrics.py --compare \
  harness/build/dino-direct-animated harness/build/dino-cull-animated
python3 harness/scripts/dino_pipeline.py --compare \
  harness/build/dino-direct-paused harness/build/dino-cull-paused
```

The game also consumes the accumulated renderer improvements in a fresh
installed-package build. Its frozen two-actor median work falls from 64 to
57 ms, geometry from 27 to 24 ms and flush from 17 to 14 ms. Full neutral work
falls from 108 to 100 ms; that comparison includes earlier renderer changes,
not just this admission increment. Game evidence lives in its private
`recovery-sdk-cull-matrix`; gameplay cadence requires separate uninstrumented
captures.

## Callers and copy entry checkpoints

`analyze_cpu_profile.py` now resolves observed call edges and exact `memcpy`
entry snapshots against the matching ELF. Call counts remain separate from
exclusive PC costs. For SH-2 BSR/BSRF/JSR, PR points four bytes after the call;
resolving PR-4 identifies the caller after its delay slot has executed. ABI
registers r4/r5/r6 provide destination, source and byte length. The report groups
requested bytes, lengths, caller, CPU, normal Work RAM regions and alignment.
These are entry arguments, not bus traffic or inferred inclusive costs.

For example, request the `memcpy` symbol checkpoint in the game's reusable
probe script and then run:

```sh
python3 harness/scripts/analyze_cpu_profile.py \
  --prefix /path/to/capture/debug --elf /path/to/matching.elf \
  --nm /path/to/sh2eb-elf-nm --out /path/to/capture/copies.json
```

The six-field Paul capture `recovery-copy-entries` observes 828 copies requesting
16880 bytes. Most calls are 18-byte rotation matrices from FK/IK and pose
blending. Different modulo-four alignments select the libc byte path even when
both pointers support halfword accesses. This identifies a concrete normal-RAM
copy experiment; it does not justify changing authoritative pose arithmetic.
The existing copy kernel already supports aligned 32-bit words. Empty checkpoint
files do not prove that no copies occurred. Seven tool tests cover aliases,
assembler boundaries, call-vs-return events, CPU separation, PR attribution,
lengths, invalid register values and alignment/region classification.

### Normal-RAM copy microbenchmark

`examples/memory_copy_bench` invokes the actual freestanding libc entry through
a volatile function pointer. It tests 512 combinations: all modulo-four source
and destination offsets, High/Low Work RAM pairs and lengths 0, 2, 8, 18, 32,
98, 183 and 704 bytes. Five local FRT intervals per case include call/loop
costs and interrupts. Source content, destination guards and the returned
pointer are checked outside the normal timed path. No asset fixture is needed.
The kernel is measured with the Makefile's stock `-O2`; installed-package game
measurements separately exercise its Release `-O3` compilation.

```sh
python3 harness/scripts/memory_copy_bench.py --bios /path/to/bios.bin \
  --out harness/build/memory-copy-before
# Rebuild after changing the libc kernel, keeping this probe unchanged.
python3 harness/scripts/memory_copy_bench.py --bios /path/to/bios.bin \
  --out harness/build/memory-copy-after
python3 harness/scripts/memory_copy_bench.py --compare \
  harness/build/memory-copy-before harness/build/memory-copy-after
```

Captures retain the image, exact ELF/BIOS/probe hashes, command, raw big-endian
records and decoded cases. Comparison rejects different firmware/emulator or
field windows, cache policy, duplicate/missing cases, failed guards and differing
checksums. It reports calibrated local intervals and grouped alignment results;
these remain emulator observations. Host oracle tests and a clean asset-free
cross-build have a dedicated `Normal RAM copy gates` workflow.

## DIVU timing limitation demonstrated independently

Before implementing division overlap, the pinned Ymir core was checked against
SH7604 manual sections 10.3 and 10.4.1. The manual specifies 39 cycles for a
normal division and extends a register access until completion. Independent
instructions may execute during those cycles. The pinned core instead calls
`Calc32`/`Calc64` synchronously; its `sh2_divu.hpp` contains the explicit TODO to
schedule completion after 39 cycles, and on-chip accesses have a fixed cost.

The new asset-free `division_latency_probe` puts exactly 0, 8, 16, 32, 48 or 64
NOP instructions between the start write and result read. Both signed 32/32
and 64/32 paths use nonoverflowing positive operands. It repeats each case 256
times and retains five local FRT intervals, prescaler, CCR and checked quotients.
A screen displays the medians so the SAME native disc can be tested without
RAM-debugger support in an independent emulator.

| Operand width / emulator | 0 NOP | 8 | 16 | 32 | 48 | 64 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 32 / pinned Ymir | 50 | 66 | 82 | 114 | 146 | 178 |
| 64 / pinned Ymir | 58 | 74 | 90 | 122 | 154 | 186 |
| 32 / Mednafen CD boot | 113 | 112 | 112 | 113 | 133 | 164 |
| 64 / Mednafen CD boot | 113 | 113 | 112 | 112 | 139 | 170 |

Values are median raw FRT ticks per 256 calls, including common loop/call
costs. They are not per-division CPU cycle counts. In Ymir, inserting early NOPs
costs just as much as inserting late ones. Mednafen exhibits the expected early
plateau: those instructions overlap the division instead of adding elapsed
time. Both return the correct quotients with zero error counters. This is an
independent-emulator consistency check against the manual, not a hardware test.

Public numeric evidence and matching image hashes are retained in
`harness/studies/divu-latency/results.json`; private raw images, RAM and recording
are `divu-latency-ymir` and `divu-latency-mednafen`. The probe now explicitly
writes `division_timing_model: immediate-result-no-busy-wait` in ordinary and
CPU-profile JSON. This labels the existing model; it does not repair the core.
Existing Ymir geometry intervals and cadence remain observations of that model.
They cannot quantify the hardware benefit of overlapping division work, and
adding a flat 39 cycles per observed divide would also be incorrect because
independent work can overlap it.

Reproduce on Ymir, then record the retained native disc in Mednafen:

```sh
python3 harness/scripts/division_latency.py --bios /path/to/bios.bin \
  --out harness/build/divu-latency-ymir
python3 harness/scripts/cube_pipeline.py --record-cd harness/build/divu-latency-ymir \
  --record-example division_latency_probe --bios /path/to/bios.bin \
  --out harness/build/divu-latency-mednafen --seconds 20
```

The three decoder tests reject invalid quotient/order, incomplete publication,
wrong FRT prescaler and invalid intervals. A dedicated workflow cross-builds the
procedural image. The harness's existing CPU-accounting integration tests pass
with the explicit model label. Fixing asynchronous DIVU completion/access waits
requires a separate core change with clock, interrupt, overflow and state tests;
no renderer scheduling change is accepted on an unmodeled timing assumption.

### Retained halfword copy kernel

The libc copy routine now handles matching pointer parity with differing
modulo-four alignment through an aligned 16-bit kernel. An initial byte aligns
odd/odd pointers; bounded halfword groups and tails never overread the requested
range. Different parity retains byte copying. A separate non-inlined halfword
kernel keeps its register pressure out of the existing 32-bit loop.

The first inline implementation (`memory-copy-halfword`) accelerated the target
alignment but regressed some short word/byte cases by 8–14%. It was replaced by
the isolated kernel (`memory-copy-halfword-v2`). All 512 target cases pass guards
and content checks against the same unchanged microbenchmark image source.
Both retained runs calibrate to 210540 Hz with the same probe and BIOS.

| High-to-High Work RAM / length | Before us/call | After us/call |
| --- | ---: | ---: |
| Halfword-only alignment / 18 | 7.834 | 4.773 |
| Halfword-only alignment / 98 | 37.552 | 11.948 |
| Halfword-only alignment / 704 | 262.495 | 65.457 |
| Matching word alignment / 18 | 4.495 | 4.587 |
| Different parity / 18 | 7.839 | 7.950 |

These are medians across the corresponding offset cases, including dispatch and
loop overhead. The 18-byte target case improves by about 39%; short unaffected
paths retain a small fixed dispatch cost, rather than a blanket speedup for all
copies. The 828-entry Paul capture includes 401 calls compatible with the
halfword path but not the old word path. The existing host oracle checks all
source/destination offsets and lengths through 1024 bytes with guard content.

The Release-package Paul matrix (`recovery-halfword-v2-matrix`) measures full
neutral simulation at 11.727 ms/tick versus 12.021 ms/tick in the preceding
renderer baseline. Full work median/p95 is 99/112 ms versus 100/113; frozen
render work remains 57 ms. A faster simulation executes a different number of
calls in a fixed field window, so the higher aggregate memcpy share in an
intermediate profile is not evidence that individual copies became slower.
This remains a small measured improvement; no new presentation-rate target is
claimed. The full host/tool suite and all five installed-package CTests pass.
