# Voxel Terrain / Voxel Space — implementation plan

Status: **initial CPU renderer + VDP1 dynamic-texture demo landed**. This plan
tracks remaining presentation, performance and depth/composition gates; it
does **not** claim support for volumetric blocks, caves or destructible 3D.

## Scope and invariants

- The first product is a **2.5D height field**: one height and one INDEX8
  palette entry per world (x,z), nearest-neighbor samples, no overhangs.
- Core CPU algorithm never accesses VDP1/VDP2 registers. It receives map
  pointers, camera, target pixels and 4-byte-aligned caller-owned scratch.
- 16.16 camera, deterministic integer projection, sample-by-distance and
  per-column near-to-far occlusion ceiling. No heap, no hidden frame buffer.
- The API distinguishes **world render time**, **VRAM upload time** and
  **total displayed frame time**, including contention and dropped frames.
- A presenter must not reinterpret a palette index 0 as opaque sky unless
  VDP1 SPD is set. For this demo sky=1 and material colors=2..8.
- Do not assume the 4MB RAM expansion behaves like contiguous main RAM:
  the exposed API has two physically distinct banks. Baseline uses none.

## Phase 0 — Presentation and timing (partially implemented)

A dynamic INDEX8 VDP1 texture is the **first known code path**; see
`examples/voxel_terrain/main.c`. Rendering to 160x112 produces 17,920 bytes;
the example uploads the complete surface through
`sat_texture_update()` and draws the texture scaled to 320x224.
This is a measurable baseline, **not** evidence of safe zero-tearing streaming
during every VDP1 drawing phase on all hardware.

A first probe is now provided at `examples/voxel_display_probe/`: the same moving\n160x112 diagnostic image can be toggled between a VDP1 dynamic sprite\nand a VDP2 512x256 RBG0 bitmap. The RBG0 prototype packs only the\n160x112 active rectangle into 80 words per row and configures kx=ky=0.5,\nso the VDP2 performs the 2x enlargement. Row uploads currently access the\nvisible bank without a frame-atomic swap: tearing and contention require\nvisual/emulator/hardware measurement; this is NOT the production presenter.\n\nRemaining probe: `examples/voxel_display_probe/` with a changing diagnostic
pattern and *separate* backend runs:
1. VDP1 dynamic texture: record CPU copy duration, VDP1 draw duration, total
   elapsed VBlanks and tearing at each target size (80x56,160x112,320x224).
2. VDP2 RBG0 bitmap: the current public RBG0 bitmap path uses at least
   512x256 indexed8 (131,072 bytes of VRAM). Prove bank reservation, valid
   rotation-table placement and palette ownership; update only the active
   rectangle using row stride, configure enlargement correctly, and measure
   CPU writes against VDP2 display-fetch contention. Do NOT claim existing
   `sat_vdp2_vram_write_words()` already handles blanking, DMA or buffering.
3. Compare full-window vs dirty rectangle, and CPU copying vs a future SCU
   DMA path **only after** that path has been implemented and verified.
4. Test visibility while VDP1 framebuffer erase is opaque and transparent;
   ensure VDP2 background is visible when expected. VDP2 backend must not
   clobber other layer registers or the sprite color-calculation state.

**Gate:** a moving 160x112 diagnostic pattern is coherent, no invalid VRAM
writes, and complete measurements of transfer bytes, CPU time, VBlank count
and visual behavior on emulator and ideally hardware. Select a default based
on measured frame time; do not call RBG0 the winner in advance.

## Phase 1 — Deterministic CPU terrain (implemented baseline)

`include/saturn/voxel_terrain.h` exposes `sat_voxel_terrain_render()`,
`sat_voxel_terrain_scratch_bytes()` and `sat_voxel_terrain_height_at()`.
`src/core/voxel_terrain_api.cpp` calls existing fixed-point sin/cos helpers,
precomputes perspective factors per depth, follows each view ray from nearest
to farthest and fills visible vertical spans above the nearest-filled limit.
The camera and target bounds keep arithmetic and scratch sizes bounded.
The target's padding must remain unchanged. Map boundaries clamp or wrap,
including correct negative coordinates and non-power-of-two dimensions.

Follow-ups:
- Property tests for monotonic occlusion, camera translation/rotation,
  negative world positions, rectangular map pitch and extreme pitch.
- Golden images at several view angles, both edge policies and target sizes;
  host comparison with a small independent reference implementation.
- Add adjustable distance stepping/LOD only with image-regression tolerance,
  profiler evidence and no holes or thin-column discontinuities.

**Gate:** host tests and all existing examples still build; renderer does not
touch global runtime state or hardware, and caller-owned memory is bounded.

## Phase 2 — Usable flight demo (first slice implemented)

`examples/voxel_terrain/` contains procedural 256x256 maps, flying camera
and VDP1 INDEX8 output with HUD measurements. Initial target is 160x112,
view distance 120; these are **benchmarks, not target-FPS claims**.

Follow-ups: camera-ground collision, frame delta-based movement, simple
palette fog, look/pitch limits derived from displayed resolution, selectable
distance and resolution profiles, overlay of native VDP1 3D meshes.

**Gate:** visible sky, land and altitude changes without a warped horizon,
smooth flyover on emulator/hardware, no memory corruption at map edges.

## Phase 3 — Video-presenter contract and synchronization

Design a small internal backend interface with explicit
`init`, `upload(source,pitch,dirty_region)`, `present`, `shutdown`.
Keep the game API and height renderer independent of the chosen backend.

- VDP1 must validate width multiple of 8 and command-pattern limits; avoid
  writes to texture VRAM while VDP1 reads that texture for the previous frame.
- VDP2 must reserve aligned bitmap/rotation VRAM and verify bank assignments,
  display fetch cycles, priority, palette and scale. The first presenter may
  occupy a 512x256 bitmap while only 160x112 pixels are generated; document
  how the active rectangle is mapped to the visible screen.
- Copy/flip only when safe; choose stall, previous-frame reuse or frame drop
  explicitly when a transfer misses the available update window.
- Do not assume double buffering is free: each full RBG0 bitmap can consume
  a 128 KiB bank. Plan VRAM ownership before allocating multiple bitmaps.

**Gate:** identical indexed images through both backends for a fixed camera,
recorded upload costs and no tearing in the hardware/emulator test matrix.

## Phase 4 — Game-object placement and occlusion

Use `sat_voxel_terrain_height_at()` for camera/vehicle clearance, placement,
projectile-ground collisions and shadow anchoring. World-space objects rendered
with VDP1 over VDP2 terrain are not automatically occluded by hills: sprite
priority cannot replace a shared depth buffer.

Start with coarse line-of-sight sampling per object; then investigate a
terrain depth/visibility representation with an explicit camera transform and
pixel precision. Document failures for large objects, hill silhouettes and
multiple objects. Sort objects back-to-front when using VDP1 half-transparency,
which only blends against earlier RGB framebuffer pixels, not the VDP2 terrain.

**Gate:** an object on a near ridge is visible and one behind it is hidden
without falsely hiding objects in open valleys.

## Phase 5 — Profiling and parallelism

Record medians and worst-frame times for ray setup, height/map sampling,
span filling, texture copying, display, input/audio and complete frame across
camera heights/distances. Check column count vs cost and VRAM bandwidth.

Only after a generic Slave SH-2 job runtime exists (see
`docs/SLAVE_SH2_PHYSICS_PLAN.md`), benchmark coarse column batches:
each column has one writer, map inputs are immutable, scratch is partitioned,
and cache coherency/completion are explicit. Do not dispatch one job per voxel.
Fallback must remain the Master SH-2 path; do not introduce silent static
constructors, allocations or indefinite busy waits.

**Gate:** repeatable complete-frame benefit after factoring shared-memory
traffic and VDP bus contention, otherwise keep the single-CPU backend.

## Phase 6 — Streaming and optional RAMCart

Add an offline heightmap/color-map importer and deterministic region atlas.
Explicitly handle tile seams, physical cartridge bank gaps, lifetime and
fallback to smaller in-RAM maps when no RAMCart is inserted. Streaming CD
assets must not force synchronous reads in the render loop.

## Out of scope for this module

True 3D occupancy grids, octrees, caves, destructible blocks and per-voxel
lighting require a separate 3D volume renderer and different visibility,
collision, storage and LOD semantics. Do not disguise height fields as full
volumetric-voxel support.

## Useful commands

```sh
make test
make EXAMPLE=voxel_terrain all
make examples-all
```

The host tests prove deterministic CPU behavior and validation only. Visual
acceptance and a measured FPS require executing the built image in an emulator
and checking a Saturn when available.
