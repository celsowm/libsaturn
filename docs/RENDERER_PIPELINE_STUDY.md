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
