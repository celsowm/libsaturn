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

- [ ] **M0 — Audit.** Trace VDP1 command-list banks and drawing/display framebuffer ownership, erase bounds, CEF/IST completion, swap serial and AUTO/ON_SUBMIT contracts. Identify where the experimental probe can defer a swap before the FBCR write. The existing swap handler runs after the request. Trace scene capture through the painter queue, the projected and clipped RGB emitters, split-face semantics, VDP2 bitmap allocation/priority/zoom and DMA restrictions. Record ownership, timing and the commands needed to reproduce each probe.
- [ ] **M1 — Baseline.** Add water_lab with main.c, scene.c, camera.c, player.c, benchmark.c, README and Makefile.inc only as needed. Build using the existing example Makefile. Render the complete dry scene, deterministic camera, HUD and baseline captures. Reuse existing scene and camera APIs. Track 128/256/512 face workloads and increase stress levels separately.
- [ ] **M2 — Native surface.** Add flat and sine-LUT animated water with bounded 4×4/8×8/16×16 subdivisions. Extend material validation and both projected and clipped RGB emission paths to support opt-in mesh and RGB half-transparency in the existing painter order. Keep opaque as the default material behavior. Check command bits and capture known RGB and indexed destinations, opaque geometry in front of and behind the surface, and a surface crossing the near plane. Compare opaque, mesh and half-transparency cost at matched ticks. Verify winding, shared vertices, views from both sides and artifacts where translucent quads overlap.
- [ ] **M3 — Underwater effects.** Split supported crossing faces in world space and retain each piece's dry or submerged classification. Reuse the existing plane-cut math where suitable. `sat_scene_submit_quad_split` queues all pieces with one material and pass and rejects textured and per-vertex Gouraud inputs, so the water path needs access to the pieces before submission. Use the same fixed-point intersections on both sides. Apply depth tint and generate Gouraud caustics after the cut, with a palette fallback. Pin the shared waterline during projection warp and keep near-plane clipping valid. Start with solid crossing faces. Keep unsupported textured crossings on the ordinary rendering path and count them in diagnostics until a texture-preserving path is qualified. Test a face crossing both planes, shared edges across adjacent faces, vertices on the waterline, extreme coordinates, and any supported textured crossings. Compare host geometry results and deterministic image captures.
- [ ] **M4 — Water volume.** Add independent bounded volume sampling (surface height, containment, depth, current, buoyancy, drag). Integrate existing physics and player state; distinguish camera and player immersion. Fixed-step tests for entry/exit, surface hysteresis, partial immersion, no-water equivalence and overflow.
- [ ] **M5 — Auxiliary framebuffer probe.** Use a standalone probe before integrating the compositor into the renderer. At 320×224, investigate x=320..479, y=0..111 in the 512×256 16-bit drawing framebuffer. Validate system/user clip, local coordinates, bounded erase and isolation from the main image. Read the drawing bank only after confirmed draw completion and before requesting a swap. Verify alternating bank reuse with serial-tagged RGB patterns and guard pixels. Read 112 rows of 320 bytes each at a 1024-byte source stride (35 KiB total). Measure a row-wise CPU copy and the two legal SCU DMA legs through aligned Work RAM-H, including list setup, both waits and any conversion. Direct SCU DMA between VDP1 and VDP2 is unavailable because both are on the B-bus. Record the worst observed transfer times and retain M2/M3 if the probe fails.
- [ ] **M6 — VDP2 composition probe.** First compose a synthetic effect in reserved NBG0/NBG1 bitmap storage, then repeat with M5's readback. Verify that indexed zero and RGB pixels with bit 15 clear are transparent, and that RGB black with bit 15 set blends as a covered pixel. Check 2× scaling, edge alignment, color ratios, priorities and HUD coverage. A 512×256 bitmap needs 128 KiB indexed8 or 256 KiB RGB16. Paired RGB16 bitmaps consume all 512 KiB of VDP2 VRAM. Account for destination stride, cycle patterns, CRAM and layer conflicts. Qualify an inactive bitmap for upload before endorsing a configuration. Test indexed8 only with measured conversion cost and acceptable captured color. Use the presentation contract below to qualify the transfer deadline and recovery.
- [ ] **M7 — Generic FX compositor.** Once M5/M6 pass, reuse eligible prepared faces in the reduced region, preserving the main list's ordering and clip state. Use painter order to draw effect surfaces and clear coverage behind opaque occluders. Define mask eligibility for cutout textures and unsupported materials. Test front/behind/crossing actors, glass, particles, near clipping and camera movement. Check for halos at opaque edges after 2× scaling. Begin with one effect layer and one ratio. Document the supported overlap and HUD cases and fall back to M2/M3 for unsupported configurations.
- [ ] **M8 — Optimization.** Measure per-stage CPU, VDP1 draw, DMA, VRAM and presented frame times; compare before/after opt-in. Try reduced effect resolution, tighter bounds, occluder culling, shared face data, DMA batching, adaptive surface tessellation and Slave SH-2 only with explicit ownership. Roll back unproven optimizations. No default renderer changes without existing regressions passing.
- [ ] **M9 — SDK and publication.** Stabilize only validated interfaces, wire runtime sources into cmake/LibSaturnSources.cmake and Makefile parity, test installed SDK C headers/consumer. Add WATER_RENDERING.md, FX_COMPOSITOR.md, WATER_BENCHMARKS.md; link example from README. Distribute original assets with license and BIN/CUE. Record emulator vs real-hardware acceptance separately.

## Multipass presentation contract

- Use an opt-in manual-swap path for the probe. Associate the drawing bank and inactive VDP2 bitmap with one scene serial. Arm the VDP1 swap only after draw completion, readback and upload all finish. Publish the matching VDP2 bitmap through the same qualified display boundary and admit the next draw only after the bank change. Only the scene and effect confirmed as visible count toward presented-frame metrics.
- If a draw or transfer misses the cutoff, retain the previous visible pair. A completed late upload may be published with its matching scene at a later boundary. On transfer failure, keep the incomplete bitmap inactive and wait for the transfer to settle before reusing its storage or the drawing bank. If no legal inactive allocation fits, or the probe cannot reliably publish both parts together, select native water.
- Force a late draw, a late upload and a transfer failure after some rows have been written in separate probe cases. Verify that the prior pair remains visible, that a later matched pair can be presented, and that mode changes drain pending work before releasing ownership. Tag the visible main scene and effect independently in captures so matching software serials alone cannot pass the test.

[VDP1 framebuffer access](sega_saturn_hardware/hard/vdp1/hon/p02_11.md) · [VDP1 swap control](sega_saturn_hardware/hard/vdp1/hon/p04_12.md) · [Current swap service](../src/hal/vdp1/vdp1.cpp) · [SCU DMA routing](../src/hal/scu/dma_logic.hpp)

## Proposed API boundaries (names provisional)

- water_volume.h: pure gameplay queries and forces, no graphics dependency.
- water.h: surface sampling and submission, submerged geometry effects and caller workspace requirements.
- fx_compositor.h: optional generic effect pass; explicit init/reserve/begin/submit/finish/present/release and diagnostics. No water dependency. Introduce public APIs only after probes determine legal contracts.
- Keep low-level framebuffer/DMA implementation internal until its hardware ownership is qualified. Return existing SAT_ERR_* codes as applicable. Release all claimed VDP2 resources on exit/mode switch; reject conflicting owners.

## Acceptance and evidence

Run host tests with `make test`. On POSIX, build with `build-example.sh`, run Mednafen with `run-example.sh`, and use `harness/run-harness.sh` for Ymir RAM, register and screenshot inspection. Use the equivalent PowerShell wrappers and launchers on Windows. Repeat hardware qualification on a physical Saturn. For deterministic test scenes, record the commit, video mode, emulator version where applicable, seed, tick, visible serials, screenshot, command counts, linker map, VRAM claims, transfer bytes and timing. Keep source assets original and generated data reproducible.

- Geometry: no cracks, overflow or unbounded allocations; correct dry/submerged classification.
- Composition passes when pixel checks show correct coverage, the primary region and guard pixels are intact, and the visible scene and effect agree across at least 3600 scripted ticks including forced misses and mode switches.
- Performance: report actual presented frames, not submissions. Aim for 30 FPS NTSC basic modes and 20–30 FPS multipass in the normal scene; targets are not guarantees. Measure 50 Hz separately. Report peak as well as typical costs and compare dry reference.
- Regression: existing host suite, scene/culling/clipping, VDP1 command/swap and VDP2 ownership tests, example builds and package consumer gates. Preserve existing default behavior.
- Hardware qualification: distinguish host, Ymir, Mednafen and physical console results; do not mark hardware accepted without physical evidence.

## First deliverable

M0 audit plus M1 dry-scene example and deterministic captures. Then M2 native water. Multipass remains experimental and gated on M5/M6; if blocked, the demo still delivers native water and underwater effects.

References: [Transparency and alpha](TRANSPARENCY_ALPHA.md), [renderer study](RENDERER_PIPELINE_STUDY.md), [API ownership](PUBLIC_API_OWNERSHIP.md), [hardware acceptance](HARDWARE_ACCEPTANCE_CHECKLIST.md), [hardware manuals](sega_saturn_hardware/).
