# Water Lab — implementation plan

Status: planned; no performance or hardware qualification claims.
Target: `examples/water_lab`, a freely distributable flooded-room demo with reusable SDK facilities for water and translucent effects. Tomb Raider and Burning Rangers are visual inspirations, not verified descriptions of their original implementation.

## Constraints

- Bare-metal Saturn, no RAM cartridge required; first target 320×224 non-interlaced, 16-bit VDP1 framebuffer.
- Reuse the current scene painter, clipping, material paths, VDP1 command lifecycle, VDP2 layer ownership and DMA services. No second general renderer, no heap in frame hot paths.
- Keep existing games and SDK consumers unchanged when opting out. Caller-owned bounded workspaces, explicit capacity errors and deterministic fallbacks.
- Read TRANSPARENCY_ALPHA.md, RENDERER_PIPELINE_STUDY.md, PUBLIC_API_OWNERSHIP.md and the vendored VDP1/VDP2/SCU manuals before hardware changes.
- VDP1 mesh is not alpha; RGB half-transparency blends only with previously drawn RGB framebuffer pixels and is expensive. VDP2 sprite color calculation does not blend arbitrary primitives inside one VDP1 framebuffer.

## Demo

A small original low-poly flooded temple: walkable perimeter, central island, stairs, submerged tunnel, columns crossing the waterline, animated crossing objects, vertical glass and a particle emitter. Start with procedural geometry and original textures. Use existing scene, input, camera and HUD facilities.

Controls: D-pad move, L/R orbit, A jump/swim up, B dive, X cycle render modes, Y change water level, Z diagnostics, Start reset. C toggles camera. Include a fixed-tick automatic camera and scripted actors for captures.

Modes: off/reference, opaque, mesh, RGB half-transparency, geometric underwater warp/caustics, experimental multipass. Unsupported modes must report why and fall back visibly. Compare identical scene states.

## Execution milestones (in order)

- [ ] **M0 — Audit.** Trace actual VDP1 frame-bank selection, erase bounds, CEF/IST completion, swap serial and AUTO/ON_SUBMIT contracts. Inspect scene capture and split-face semantics, textured clipping limitations, VDP2 bitmap allocation/priority/zoom and DMA transfer restrictions. Record exact ownership and timing; do not assume scene capture is a render target.
- [ ] **M1 — Baseline.** Add water_lab with main.c, scene.c, camera.c, player.c, benchmark.c, README and Makefile.inc only as needed. Build using the existing example Makefile. Render the complete dry scene, deterministic camera, HUD and baseline captures. Reuse existing scene and camera APIs. Track 128/256/512 face workloads and increase stress levels separately.
- [ ] **M2 — Native surface.** Add flat and sine-LUT animated water with bounded 4×4/8×8/16×16 subdivisions, mesh and RGB half-transparency modes. Verify winding, shared vertices, color-code compatibility (RGB vs indexed), camera above/below, screen coverage and quad overlap artifacts. Compare opaque/mesh/half cost. Do not promise arbitrary alpha.
- [ ] **M3 — Underwater effects.** Split crossing faces into dry/submerged pieces (reuse sat_scene_submit_quad_split where suitable; explicitly handle UV restrictions). Add fixed-point shared-vertex projection warp, depth tint and animated Gouraud caustics or palette fallback. Preserve valid clipping and depth ordering; test near plane, shared edges, texture continuity and extreme coordinates. Warp is an approximation, not framebuffer refraction. Test host arithmetic and image captures.
- [ ] **M4 — Water volume.** Add independent bounded volume sampling (surface height, containment, depth, current, buoyancy, drag). Integrate existing physics and player state; distinguish camera and player immersion. Fixed-step tests for entry/exit, surface hysteresis, partial immersion, no-water equivalence and overflow.
- [ ] **M5 — Auxiliary framebuffer probe.** Standalone hardware test before changing renderer. At 320×224, investigate x=320..479, y=0..111 in a 512×256 16-bit framebuffer. Validate system/user clip, local coordinates, bank addresses, bounded erase, completion, access permissions and independent scene contents. Prove row-strided readback with known RGB patterns, then transfer through supported DMA or CPU fallback. A 160×112 rectangle has 35 KiB of payload but 1024-byte source row stride; never copy as a contiguous rectangle. Reject unsafe reads during drawing/swapping. If lateral access is unavailable, document alternatives; stop rather than corrupt the primary frame.
- [ ] **M6 — VDP2 composition probe.** Transfer a synthetic effect into reserved NBG0/NBG1 bitmap storage, enable supported color calculation with that layer above the VDP1 sprite layer, verify zero/transparent pixels, RGB code, 2× scaling, priorities, ratios and HUD behavior. Qualify a complete frame serial from VDP1 completion to corresponding VDP2 presentation. Do not present stale effect pixels over a newer scene. A 512×256 bitmap requires 128 KiB indexed8 or 256 KiB RGB16; double RGB buffering consumes all 512 KiB VDP2 VRAM. Account for destination stride, cycle patterns, CRAM, layer conflicts and safe update scheduling. Evaluate indexed palettes only if conversion cost and color fidelity pass. Keep fallback to M2/M3.
- [ ] **M7 — Generic FX compositor.** Once M5/M6 pass, lower eligible prepared faces to the reduced region; preserve main command ordering and state. Draw transparent surfaces plus ordered opaque masks (color-key clearing) according to painter depth. Reuse prepared geometry instead of transforming everything again. First support a single effect layer and one ratio; document cases of intersecting surfaces, overlapping alpha and HUD that cannot be represented. Test front/behind/crossing actors, glass, particles, near clipping and camera movement. No claim of order-independent transparency.
- [ ] **M8 — Optimization.** Measure per-stage CPU, VDP1 draw, DMA, VRAM and presented frame times; compare before/after opt-in. Try reduced effect resolution, tighter bounds, occluder culling, shared face data, DMA batching, adaptive surface tessellation and Slave SH-2 only with explicit ownership. Roll back unproven optimizations. No default renderer changes without existing regressions passing.
- [ ] **M9 — SDK and publication.** Stabilize only validated interfaces, wire runtime sources into cmake/LibSaturnSources.cmake and Makefile parity, test installed SDK C headers/consumer. Add WATER_RENDERING.md, FX_COMPOSITOR.md, WATER_BENCHMARKS.md; link example from README. Distribute original assets with license and BIN/CUE. Record emulator vs real-hardware acceptance separately.

## Proposed API boundaries (names provisional)

- water_volume.h: pure gameplay queries and forces, no graphics dependency.
- water.h: surface sampling and submission, submerged geometry effects and caller workspace requirements.
- fx_compositor.h: optional generic effect pass; explicit init/reserve/begin/submit/finish/present/release and diagnostics. No water dependency. Introduce public APIs only after probes determine legal contracts.
- Keep low-level framebuffer/DMA implementation internal until its hardware ownership is qualified. Return existing SAT_ERR_* codes as applicable. Release all claimed VDP2 resources on exit/mode switch; reject conflicting owners.

## Acceptance and evidence

Run host tests (make test), example build, Ymir headless harness, Mednafen and eventually physical Saturn. Add deterministic test scenes and capture metadata: commit, video mode, emulator version, seed, frame serial, screenshot, command counts, linker map, VRAM claims, transfer bytes and timing. Keep source assets original and generated data reproducible.

- Geometry: no cracks, overflow or unbounded allocations; correct dry/submerged classification.
- Composition: expected transparent/opaque pixels, no wrong-bank writes, stale masks, tearing or main-region corruption; at least 3600 scripted ticks of stable switching.
- Performance: report actual presented frames, not submissions. Aim for 30 FPS NTSC basic modes and 20–30 FPS multipass in the normal scene; targets are not guarantees. Measure 50 Hz separately. Report peak as well as typical costs and compare dry reference.
- Regression: existing host suite, scene/culling/clipping, VDP1 command/swap and VDP2 ownership tests, example builds and package consumer gates. Preserve existing default behavior.
- Hardware qualification: distinguish host, Ymir, Mednafen and physical console results; do not mark hardware accepted without physical evidence.

## First deliverable

M0 audit plus M1 dry-scene example and deterministic captures. Then M2 native water. Multipass remains experimental and gated on M5/M6; if blocked, the demo still delivers native water and underwater effects.

References: [Transparency and alpha](TRANSPARENCY_ALPHA.md), [renderer study](RENDERER_PIPELINE_STUDY.md), [API ownership](PUBLIC_API_OWNERSHIP.md), [hardware acceptance](HARDWARE_ACCEPTANCE_CHECKLIST.md), [hardware manuals](sega_saturn_hardware/).
