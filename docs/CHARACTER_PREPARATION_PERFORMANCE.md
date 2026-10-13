# Character preparation performance

This follow-up compares two HIGH-detail original characters with the same
textures, topology, camera, walk clip and 60 Hz simulation. It follows the
[initial readiness report](FIGHTING_GAME_READINESS.md); it does not reduce LOD
or omit simulation work to improve the display counter.

## Changes and safety contracts

The shared character example now uses the renderer's existing immutable
instance binding. Validation occurs on LOD binding and resource reload, rather
than repeating the complete index/material-map check every rendered frame.
Animated vertex positions may change in place. Buffer pointers/counts, topology,
material table and selectors stay fixed until a binding is rebuilt. Hit-flash
and RGB Gouraud tables remain on the full validation path because their contents
change. GPU texture residency checks and global painter ordering remain in place.

World-space vertex preparation calls the existing fixed-point matrix helper
directly. The compiler can eliminate the unused fourth output component and
fold the constant homogeneous input into translations. Each multiplication
still rounds before summation. This does not fuse matrices, change projection
precision or replace geometric visibility with rounded screen winding.

The host scene API suite checks 64 fractional/negative/non-affine matrices against
an independent per-product integer reference. Existing immutable-binding,
capacity, clipping, sorting and batch-equivalence checks remain active. The
skinned-scene interleaving suite also passes. Emulator regression scenarios cover
hit flashes, guard, attacks, resource reloads, all nine viewer clips and RGB
lighting, including the binding's mutable-material fallback.

The full host/tool gate and all three stock-RAM ISO builds passed. Ymir reran
the 1,150-field arena and 1,860-field viewer scenarios with zero errors/panic;
paused viewer pixels matched. A separate 520-field 1→2→4→8 actor sequence
accepted all eight LOW-detail actors after automatic LOD changes, reporting
eight explicit downgrades and 850 commands. Mednafen displayed the optimized
arena without an expansion cartridge. Physical Saturn acceptance remains open.

## Reproduction and interpretation

```powershell
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario prepare
```

The optional `prepare` scenario uses 1,400 NTSC fields. It selects two actors,
then AUTO, MASTER and SLAVE in consecutive intervals. Analysis discards four
transition samples and requires at least twenty steady samples per policy,
all actors accepted, zero panic/errors, stock RAM and serial/worker equivalence.
Simulation rate and submission throughput use observed VBlank differences;
median CPU/preparation/skin/frame costs and the complete-frame p95 are reported.
Submission/s is not distinct presented images/s or a physical hardware claim.

The baseline is commit `0851cb4`. Comparison maps, RAM dumps, JSON, logs and
captures are in `harness/build/character_prepare_baseline*` and
`character_prepare_optimized*`. The ordinary scenario writes
`harness/build/character_prepare*`. Compare the same build settings and input
script when repeating on a different emulator or physical Saturn.

## Measured results

| Policy | Baseline CPU / preparation | Optimized CPU / preparation | Baseline / optimized submissions/s | Optimized frame p95 |
| --- | --- | --- | --- | --- |
| AUTO | 56 / 29 ms | 50 / 23 ms | 15.00 / 15.08 | 68.00 ms |
| MASTER | 60 / 29 ms | 54 / 22 ms | 15.00 / 15.00 | 68.00 ms |
| SLAVE | 60 / 29 ms | 54 / 23 ms | 15.00 / 15.00 | 68.05 ms |

The AUTO CPU reduction is approximately 11%; preparation drops approximately
21%. Skin costs remain 5 ms AUTO and 9 ms MASTER/SLAVE. The optimized runs
recorded 97/96/120 steady samples respectively. Simulation remains about 60
steps/s (60.00/60.16/60.13 measured). Median commands remain approximately
610–612; pose timing changes individual visible-face counts without changing
the geometry, LOD or face-admission algorithm.
Repeating through the committed `prepare` scenario produced identical workload
sample counts, medians, p95 values and VBlank-derived rates.

The complete frame median remains 67 ms. The small AUTO throughput difference
is not evidence of a new display-rate tier. Faster preparation creates CPU
headroom, but 20/30 submissions/s has not been reached. The next investigation
should measure VDP1 draw and handoff waits alongside remaining geometry and
emission cost before selecting another workload partition. There is no promise
of a hardware FPS improvement from this CPU result.

The subsequent [handoff study](CHARACTER_HANDOFF_PERFORMANCE.md) records
VBlank/submission timings and compares CPU versus SCU-DMA command transfer.
DMA increased Ymir submission cost without improving throughput, so the
examples retain CPU command uploads. Coarse geometry preparation is the next
measured target.

Each actor's immutable binding adds 36 bytes of caller-owned storage. The
eight-actor stock build leaves 43,160 bytes above BSS, including the enforced
16 KiB Master stack reserve. No additional worker queues, texture uploads or
per-frame allocations are introduced.
