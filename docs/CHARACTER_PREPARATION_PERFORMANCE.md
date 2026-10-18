# Character preparation performance

This follow-up compares two HIGH-detail original characters with the same
textures, topology, camera, walk clip and 60 Hz simulation. It follows the
[initial readiness report](FIGHTING_GAME_READINESS.md); it does not reduce LOD
or omit simulation work to improve the display counter.

## Sparse camera batch projection (2026-10-17)

The sibling Tekken port's live 1,200-frame profile attributed 24% of sampled
PCs to vertex projection. `sat_project_vertices` now classifies the matrix once
per batch. For a camera whose right axis is world X, four coefficients are zero;
the batch omits those products and the unused clip-Z row. Every retained fixed
product still rounds before addition. Other camera matrices use the existing
scalar path. There is no cache across calls, so camera updates and independently
animated instances cannot retain an old classification. Behind-camera vertices
still update W while leaving X/Y untouched.

A 96-matrix × 257-point differential regression exercises fractional positive
and negative coefficients, all four fallback-triggering coefficients, clipping,
behind-camera points and empty input. The render logic suite passes 38 cases.
The complete supported native host/tool suite passes on macOS; the Saturn WRAM
mapping ABI test explicitly returns unavailable there (exit 77). Package host
CTest passes 3/3. The SH-2 SDK builds/installs, and `basic_3d_texture` builds with
GNU Make and runs for 240 injected Ymir frames with an inspected textured model.

In the port's explicitly labeled frozen-pose benchmark at tick 120, final-instance
projection changes **1,017 → 962 FRT ticks** (5.4%); preparation remains 2,561,
command count 793, full render 66 ms and screenshot RGB pixels identical. This
is a small projection improvement, not a whole-frame 5.4% speedup. Evidence is
private in the sibling port's `assets/imported/paul-projection-bench-{before,final}/`
and `paul-projection-comparison.json`.

The rebuilt live port passes all 24 combat/round/replay scenarios. Its 600-VBlank
interval still delivers **8.1 submissions/s and 32.4 simulation steps/s**; this
change does not close its 30 Hz render / 60 Hz logic target. Further optimization
must address measured scene/material and application simulation costs.

The package flag-parity test also now recognizes the existing, matching
`faces.cpp`-only `-O3` override in Make/CMake. Previously it rejected that intended
exception in CI. It still requires `-O2` everywhere else and verifies that the
Make rule contains the exception. Local compile-database parity passes for
154 C++, two C and three assembly sources.

## Changes and safety contracts

The shared character example now uses the renderer's existing immutable
instance binding. Validation occurs on LOD binding and resource reload, rather
than repeating the complete index/material-map check every rendered frame.
Vertex positions and buffers may change while validated counts, topology,
material table and selectors stay fixed until a binding is rebuilt. The later
[topology/pose refactor](SHARED_RENDERER_PERFORMANCE.md) also permits independent
mesh descriptors and capacities covering the validated counts. Hit-flash
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

At the time of this study, each actor's immutable binding added 36 bytes of
caller-owned storage; the later topology/pose refactor removes four identity
fields. The
eight-actor stock build leaves 43,160 bytes above BSS, including the enforced
16 KiB Master stack reserve. No additional worker queues, texture uploads or
per-frame allocations are introduced.

## Dense projected-face preparation (2026-10-16)

A separate consumer test in `tekken3-saturn` isolates rendering at a fixed
authored pose. It freezes simulation explicitly and labels its image
`RENDER BENCHMARK`; it is not a new simulation-rate or playable-performance
claim for the arena measurements above.

Dense instances now scan the projected mesh bounds once. If every vertex meets
the existing depth/coordinate rules, faces can reuse that proof. RGB and indexed
coordinate limits remain distinct. When the proof fails, each face retains the
original checks; an unused off-screen vertex cannot reject valid faces.
World-normal visibility, global painter order, clipping fallbacks, residency
checks and queued material ownership remain unchanged. No scratch allocation or
public ABI addition is needed. Both build systems compile only `scene/faces.cpp`
with `-O3`; other runtime sources retain their existing optimization level.

| Fixed-pose consumer variant | Render/HUD work sample |
| --- | --- |
| Previous `-O2` face module | 73 ms |
| `-O3` face module | 68 ms |
| `-O3` plus projected-bounds proof | 66 ms |

All variants reach the same simulation tick 120, submit 793 commands, and
produce identical decoded screenshot pixels. Their observed frozen-loop rate
remains 12/s, so the CPU saving is not presented as a display-rate improvement.
This work sample includes skinning and HUD. Separate FRT profiling of the live
baseline attributes about 14 ms of preparation and 4.5 ms projection to the last
actor, and about 11 ms to full painter/emission work (NTSC FRT divided by 128).

The host API suite compares dense instance submissions against individual-quad
submissions across 48 RGB/indexed boundary cases, including near clipping and
an unused invalid vertex. Existing binding, atomicity, world visibility and
parallel merge checks remain active. The API suite also passes with `-O3` and
ASan/UBSan. These emulator/host results do not establish physical Saturn timing.
