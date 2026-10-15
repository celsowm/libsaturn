# 30 FPS character rendering roadmap (pre-Tekken 3 port)

Status: **planned / not yet implemented** (2026-10-14). This is a performance-gated roadmap, not a claim that the renderer achieves 30 unique presented FPS with MEDIUM or HIGH character models.

## Goal and constraints

Before attempting a fighting-game port, make the *generic* LibSaturn two-character rendering and asset stack predictable at a sustained NTSC two-field presentation cadence. The deterministic gameplay simulation stays at 60 Hz even if presentation falls behind; never delete simulation ticks, reduce collision precision or silently downgrade visual detail to meet a counter target.

Targets:

- Baseline and test on the stock-RAM `fighter_arena_3d` and `skinning_stress_3d` examples, with exactly two fighters at LOW, MEDIUM and HIGH, in MASTER/SLAVE/AUTO.
- Aim for 30 **distinct** presented frames/s on physical NTSC hardware for MEDIUM; investigate HIGH without claiming its attainment in advance. Treat 30 *submissions*/s measured in the emulator as an intermediate gate only.
- Preserve the no-expansion-cartridge configuration, bounded memory, independent 60 Hz simulation, VDP1 command capacity, deterministic gameplay, and stable Master/Slave output.
- No change to the library's fixed-point semantics, interface ABI, cache publication contract, global painter order, texture ownership, or frustum safety without exact regression coverage.
- Keep Tekken-specific logic, proprietary data, move formats and PS1-GTE rounding in a future separate consumer repository. LibSaturn should expose generic primitives only.

## Existing measurements and already completed work

Baseline source: [Character presentation and PAL acceptance](CHARACTER_PRESENTATION_ACCEPTANCE.md), together with [paired geometry](CHARACTER_GEOMETRY_PERFORMANCE.md), [initial readiness](FIGHTING_GAME_READINESS.md), [preparation](CHARACTER_PREPARATION_PERFORMANCE.md) and [handoff](CHARACTER_HANDOFF_PERFORMANCE.md).

Measured sustained NTSC examples (Ymir, **submission** rate, not presentation rate):

| Detail | Simulation steps/s | Submissions/s | CPU median | Geometry preparation median | Emission median |
| --- | ---: | ---: | ---: | ---: | ---: |
| LOW | 60.00 | 29.66 | 26 ms | measure with matched profiling | measure with matched profiling |
| MEDIUM | 60.00 | 19.775 | 36 ms | 12.121 ms | 5.960 ms |
| HIGH | 60.00 | 19.747 | 42 ms | 15.450 ms | 7.067 ms |

PAL and separately captured detail-switch measurements must not be mixed with those sustained NTSC runs. A 33.33 ms submission interval is only a necessary interim target: real VDP1 rendering and display-field ownership must also be assessed for **unique** frames.

Do not reimplement optimizations already in main: immutable scene binding, precomputed per-vertex projection, the SH-2-assisted 64/32 projection divide, paired Master/Slave geometry preparation, source-cache publication, RGB hardware clipping when safe, CPU command transfer selected over slower DMA, scene-wide painter sorting, no per-frame heap allocations and the existing measured LOW tier.

## Phase 0 — reproducible profiling first

Run the unchanged project scripts and archive exact commit SHA, executable hash, geometry/detail setting, BIOS, video mode, cache and cart configuration, Ymir and Mednafen versions, headroom from map files, complete counters and representative screenshots.

```powershell
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario rate -RateDetail low
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario rate -RateDetail medium
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario rate -RateDetail high
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin -Scenario materials
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_eu.bin -Pal -Scenario rate -RateDetail medium
```

Keep per-iteration FRT measurement for *same* frames: hierarchy evaluation, clip sampling, skinning, world transform, projection, face culling, near/screen fallback, material/Gouraud preparation, painter ordering, command generation, command upload, wait/swap and VDP1 command/draw completion. Wall-clock stages may overlap across SH-2s and must not be summed as independent savings. Distinguish list submissions, video fields and genuinely new displayed frames. Validate instrumentation overhead separately. No performance change is accepted without its own matched before/after comparison and a reproducible harness command.

**Gate P0:** repeated baseline metrics and the largest costs identified on representative frames, no regression from instrumentation.

## Phase 1 — geometry preparation and projection

Start at `src/graphics/3d/scene/faces.cpp`, `src/graphics/3d/scene/parallel.cpp`, `src/graphics/3d/rendering/logic.hpp`, and `examples/characters/demo.c`.

1. Attribute actual work between CPU skinning, per-vertex world/projection transforms, projected and world-space backface checks, material lookup, clipping preparation, and shared-memory/cache publication.
2. Benchmark transformations with the real SH-2 compiler; avoid added 64-bit software divisions, additional passes, temporary allocations or larger caller scratch arrays.
3. Keep rounded projected winding and geometric quad visibility as distinct correctness checks. Do not remove one because a visible frame looks acceptable.
4. Avoid repeatedly publishing **provably immutable** arrays only if ownership, explicit invalidation and cross-CPU visibility are maintained and tested (the existing cached binding is not a blanket exemption).
5. Compare serial and paired worker geometry **field by field**: face admission, keys, material references, culling, clipping, Gouraud, projected corners and near-plane fallback.

**Gate P1:** strict output equivalence in host and Ymir regression scenarios; measurable reduced CPU/prepare median and p95 for two MEDIUM/HIGH characters; no extra drops, panics, unsupported task errors or RAM budget overruns.

## Phase 2 — VDP1 emission and frame scheduling

Measure `src/graphics/3d/scene/faces.cpp` emission and the VDP1 HAL. Separate frontend command construction/upload time from hardware draw completion, VRAM writes, and VBlank waits.

- Evaluate material/texture binding reuse and the projected-safe vs clipped fallback command paths on real two-character content. Preserve texture generation and residency validation.
- Do not change painter ordering or perform partial VDP1 scene commits after a capacity error. Check command rollback, overlay reservations, Gouraud/table bounds, and near clipping.
- Do not switch to SCU DMA by default: prior measurements found CPU uploads faster for this workload; require a new matched test before revisiting this.
- Optimize by measured cost, not by counting API calls. Track command count and near/screen fallback rates alongside frame time.

**Gate P2:** sustain the LOW baseline and materially improve the MEDIUM/HIGH CPU and submission tier, with no visual/order/overlay regressions and no draw timeouts or late swaps. Advance to a physical-console 30-FPS claim only after *unique frame* verification.

## Phase 3 — optional conservative pre-skin visibility

The existing instance AABB test runs when submitting geometry: by then the actor may already have paid the skinning cost. Investigate an **optional** per-actor, per-clip or per-joint conservative bound that can reject wholly off-screen actors *before* skinning and geometry preparation. Avoid any negative result for visible limbs, attacks, throws, extreme poses, camera movement or near-plane straddles. If validation overhead exceeds savings for two visible fighters, leave this optimization disabled.

**Gate P3:** no false-negative rejects across randomized poses and camera cases, identical visible-frame output, bounded memory and positive measured net benefit on an explicitly off-screen workload. Do not claim this improves two simultaneously visible fighters.

## Phase 4 — compact random-access animation

The existing study reports a lossless constant-channel opportunity: resident clip poses dropped analytically from 71,520 B to 16,464 B, but the decoder is **not** integrated or SH-2 benchmarked. Prototype the codec in `tools/model_pipeline/` first, then test an opt-in, versioned on-disc representation that decodes into caller-owned pose memory.

- Store constant channels once; store frame-varying channels so an arbitrary frame can be decoded in O(joints) (or O(active channels)) without walking all preceding frames.
- Preserve exact signed integers / quaternion channels, clip events, root motion, wrap behavior and arbitrary move restarts. Avoid interpreting sampled PS1 move channels as generic normalized quaternion clips in the future game adapter.
- Measure decoder CPU, bandwidth, compression ratio, worst-case size and peak working-set. Retain uncompressed SKA1 as a supported fallback until an explicit compatibility transition.
- Validate host roundtrip for every pose, random frame jumps, truncated/corrupt payload rejection and representative on-SH-2 runtime tests.

**Gate P4:** bit-exact pose reconstruction, predictable decode cost, reduced resident bytes, no measurable combat/handoff regression.

## Phase 5 — asset residency and CD streaming

Define bounded per-fighter active-move caches; preload likely next clips during safe periods, preserve deterministic move-frame access and permit deferred release at frame boundaries. Do not make gameplay depend on CD read completion. Run the existing real CD-backed character path in Mednafen before changing cache/streaming policies, and explicitly verify optical-disc behavior on physical hardware.

Measure worst-case round restart, character switch, clip miss, cold/warm cache, VRAM texture ownership, fragmentation, loading pauses and RAM/cart independent builds.

**Gate P5:** playable two-character stock-RAM run, deterministic input trace under normal/worst-case streaming, no asset dangling pointers, no frame-time spikes outside explicitly signalled loading transitions.

## Exit criteria before a Tekken port

1. A reproducible two-MEDIUM-fighter benchmark with baseline/optimized captures, RAM maps, task serial/worker equivalence and gameplay simulation at 60 Hz.
2. An honest reported rendering tier: separate submission throughput, VDP1 draw completion and unique physical frames. If 30 MEDIUM unique frames/s is not attained, record measured limits and the exact next bottleneck instead of weakening the gate.
3. Exact and sufficiently cheap random-access animation with bounded per-character residency, or a documented uncompressed fallback.
4. The complete host/tool/build gate, Ymir automated scenarios, Mednafen disc-backed run and physical NTSC/PAL checks in [hardware acceptance](HARDWARE_ACCEPTANCE_CHECKLIST.md).
5. The portable generic code remains in LibSaturn; the eventual Tekken consumer owns its converter (using a personally supplied disc), PS1 4.12/GTE compatibility, fight state, 22-part model semantics, gameplay, media, stages, and test traces.
