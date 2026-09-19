# Breaking refactor — initial baseline and first execution slice

Date: 2026-09-19. Base commit: ebda3dc64dbbb6ecabbd2de9c485b06a1d922db7.
Plan: [EXAMPLE_DRIVEN_BREAKING_API_REFACTOR_PLAN.md](EXAMPLE_DRIVEN_BREAKING_API_REFACTOR_PLAN.md).

## Repository audit, verified from main

- Core mesh geometry: `include/saturn/mesh3d.h`, `src/core/mesh3d_logic.hpp`, `src/core/mesh3d_api.cpp`.
- Host mesh tests: `tests/host/test_mesh3d_logic.cpp` (29 test invocations at baseline), `tests/host/test_mesh3d_textured.cpp`.
- Skybridge draw path: `examples/skybridge_3d/main.c`: per-frame manually assembled octahedron; indexed face clipping; 4 courses in `game.h`.
- Other audit targets: Pac-Man 3D static bake and per-frame primitives, Infinite Explorer world/perspective and model transforms, two duplicated Basic 3D orbit implementations, Distance Fade material plumbing, Pac-Man 2D sprite preparation, RBG0 example cross-includes.
- CI: `.github/workflows/phase3-final-gates.yml` runs `make test`, stock target toolchain build and example builds on pushes modifying core/examples/tests. `.github/workflows/harness.yml` triggers separately for harness changes.
- No Saturn SH-2 cross compiler or emulator was available in the execution environment at kickoff. Git clone to local container failed because github.com DNS was not reachable; do not claim local builds or visual qualification from this session. Use actual GitHub CI results if and when available.

## First implementation slice

- Add the canonical six-vertex/eight-facet octahedron geometry primitive with independent equatorial radius and half-height, deterministic outward winding, positive-extents validation and atomic capacity rejection.
- Initialize exactly one local-space gem mesh at Skybridge startup and reuse its indexed facets for each pickup. Preserve existing near/screen clipping and indexed color-calc draw path while unified scene/material pipeline is under construction; manual painter order is **still outstanding** and must be eliminated by the renderer workstream.
- Extend host mesh tests for counts, winding, ordered facet pairs, invalid geometry and capacity behavior.
- This is a breaking-refactor migration **slice**, not an assertion that the final new geometry/scene APIs or the entire plan are complete. The current mesh API is subject to further replacement without aliases or compatibility commitment.

## Validation record

- Initial code-slice structural checks: compare new function declarations against implementation and test call sites; CI and emulator outcomes to be recorded after actual execution.
- TODO: attach workflow URL and host/build results; capture Skybridge gem near-plane / camera-only orbit / Course 1 supporting platform and HUD visuals; record exact SH-2 resource delta.

## Second slice: renderer-owned indexed solid primitives

- `sat_draw_indexed_solid_quad3` now performs near-plane/screen clipping,
  projection and indexed uniform-material command submission for any solid
  world quad; Skybridge's old hardware-oriented `put_quad` is replaced by a
  small material selection wrapper. The UV-unsafe patterned floor path remains
  conservatively unchanged.
- `sat_draw_indexed_solid_mesh3` renders one immutable mesh with per-instance
  translation and per-face indexed materials. It pre-validates all indices,
  uses caller-owned scratch for O(n log n) painter sorting, then shares the
  same clipping and indexed VDP2 fade path.
- Skybridge's `draw_gem` no longer manually generates triangles, selects
  facet draw order or calls the raw VDP1 emitter. Its eight gem face materials
  are mapped once at startup; all pickups reuse the same local mesh.
- This is an intermediate indexed-solid subsystem, not a claim that patterned
  UV clipping, scene-wide interpenetration, global palette pooling or the
  breaking replacement of the generic mesh API are complete.

## Third slice: patterned indexed insets — renderer-owned conservative policy

- Added `sat_draw_indexed_textured_quad3`: preserves the original four VDP1
  texture corners when ALL source vertices are in front of the near guard
  and the projected quad is fully inside the physical screen. The routine
  makes no fake UV-clipping claim: unsafe insets are omitted over the already
  drawn, safely clipped solid backing face. It uses the same optional VDP2
  color-calc slot and reports whether the sprite was actually submitted.
- Skybridge's 50-line bespoke near-clip/screen-clip/VDP1 inset branch is
  replaced with a material selection and one call to the new library path.
- Host tests cover unchanged source corners, one-corner near crossing,
  screen overflow, faded/opaque submission and capacity/invalid-input handling.
- Arbitrary UV-preserving clipped patterned geometry remains **unimplemented**:
  true support requires clipped texture-region materialization or another
  verified VDP1 mapping strategy. This is not equivalent to a general
  perspective-correct UV renderer.

## Fourth slice: promote RBG0 perspective math into the library

- Moved the single host-testable Mode-7 coefficient/rotation-table
  implementation into `include/saturn/vdp2_rbg0_ground.h`. Public helpers
  now have the `sat_vdp2_rbg0_ground_` prefix; the example-local header is
  deleted, with no alias or compatibility shim.
- Migrated Skybridge, vdp2_rbg0_ground, vdp2_nbg0_rbg0_combo and the
  RBG0 host tests. `saturn/saturn.h` now exposes the canonical shared math.
- Hardware register writes, VRAM layout and horizon synchronization are a
  subsequent VDP2 runtime workstream, not a property of this pure math move.

## Fifth slice: pre-baked 2x2 textured-region fallback

- Added `sat_draw_indexed_tiled_quad3`: a fully safe quad uses one original
  full patterned sprite; when unsafe, library tests up to four midpoint
  subquads with corresponding previously uploaded 2x2 pixel regions. Every
  actually emitted tile retains its own complete UV range; anything touching
  near/screen boundaries is omitted over the existing clipped solid backing.
  No per-frame source crop or VRAM upload, at most four sprite commands.
- Skybridge creates twelve 8x8 INDEX8 texture regions ONCE from its three
  existing 16x16 paving motifs (768 additional VDP1 VRAM bytes), and passes
  those immutable region descriptors to the library for floor insets.
- This preserves the native VDP1 region mapping on axis-aligned inset planes;
  it does NOT claim arbitrary projective UV clipping, pixel-perfect seam
  handling for non-affine surfaces, or exact visibility when the near plane
  crosses a tile. True UV clipping remains a separate materialization design.

## Sixth slice: quadrant preparation belongs to the renderer

- `sat_upload_indexed8_quadrants` validates native VDP1 tile dimensions,
  source pitch and caller-owned scratch capacity before copying source
  pixels into four correct 8x8 (or larger) packed regions at startup. It
  uploads once through the existing native indexed8 texture uploader.
- Skybridge no longer contains the 2x2 pixel-crop/stride code; game content
  passes original image, palette bank, output descriptors and 64-byte scratch.
- Host tests verify byte-for-byte quadrant membership on pitched source
  images and that invalid shapes or insufficient scratch do not upload.
- Hardware upload failures after partial preparation leave allocated VDP1
  bytes; run this only during asset initialization and stop on error. This
  avoids pretending the low-level append-only VRAM uploader supports rollback.

## Seventh slice: model-aware shared orbit camera

- `saturn/orbit_camera3d.h` derives target and framing from an entire bounds box, owns yaw/pitch/zoom and projection, validates numeric bounds, and keeps fixed-point state caller-owned.
- Static Sonic and animated model examples use the same runtime with distinct fitting factors and A/C auto-orbit buttons. The animated example still uses union clip bounds; HUD, animator and model decode remain game concerns.
- Missing third-party `male_basic_walk_30_frames_loop.glb` is explicitly skipped ONLY by CI when absent. `make examples-all` still requires it without an opt-in `EXAMPLES_ALL_SKIP`.

## Eighth slice: VDP1 HUD command reservation

- All six primitive command allocation sites in the VDP1 HAL now share a quota-aware capacity guard. The new public `sat_vdp1_reserve_overlay_commands` partitions a frame after the setup commands and preserves the mandatory END entry; `sat_vdp1_overlay_begin` permanently unlocks the reserved slots for the final HUD pass, resetting next frame.
- Skybridge reserves 192 commands before clouds/world rendering, treats world SAT_ERR_CAPACITY as an optional geometry drop instead of aborting, and explicitly starts the protected HUD pass. This prevents WORLD command exhaustion from consuming the HUD's reserved command slots. It does not promise a VDP1 raster-time guarantee or protect the pig when the world itself uses every world slot.
- HAL host tests force the cap for sprite, polygon and clip primitives, confirm overlay slots remain usable, and verify per-frame reset and unsatisfiable reservation rejection. Emulator captures and measured maximum HUD glyph consumption remain outstanding.

## Ninth slice: VDP2 bitmap uploads are game-independent

- `sat_vdp2_bitmap_upload_indexed8` generates an INDEX8 image a row at a
  time into caller-owned scratch, packs the pixels into Saturn's high/low
  byte order, validates the **entire** VRAM interval before first upload,
  and writes through the public checked VDP2 API. No full-frame bitmap in
  work RAM or direct VDP2 MMIO in the migrated generator.
- Skybridge ocean (with its existing progress UI), Infinite Explorer terrain,
  RBG0 ground and NBG0/RBG0 combo all reuse the same streaming method.
  Games only provide their procedural or authored pixel selection.
- CI first-failure extraction no longer mistakes the importer report
  `animated surface error: max ...` for a compiler diagnostic. The next
  independently uncovered gate is Infinite Explorer WRAMH overflow
  (466072 bytes in run 35465179593); this image-upload refactor does NOT
  imply that the memory overrun is resolved.

## Tenth slice: remove direct register-memory writes from VDP2 examples

- Both `vdp2_rbg0_ground` and `vdp2_nbg0_rbg0_combo` now submit their
  coefficient table, rotation parameter block and per-frame camera offsets
  through the checked public `sat_vdp2_vram_write_words` path. The
  deliberately low-level coefficient *math* remains visible in the tests.
- No direct `0x25E00000` dereference remains in the four migrated
  application/probe examples. This does not alter the scene's existing VDP2
  register shadow, bank allocation or alpha semantics.

## Eleventh slice: indexed box and animated-model instance ownership

- `sat_draw_indexed_box3` takes an axis-aligned solid with a walkable top,
  half-extents, and three indexed material descriptors. The renderer selects
  the camera-facing X/Z sides, preserves outward winding, and clips each face
  through the existing tested indexed quad path. This removes the Skybridge
  `box3` wrapper's hand-built sides/top. Level-owned patterned insets and
  actual collision holes remain authored by Skybridge.
- `sat_anim_prepare_model_instance` preflights mesh/clip/shade capacities,
  decodes the current immutable LOCAL pose, applies a world matrix **once**,
  and optionally emits per-face shade-material indices. Skybridge pig now
  delegates its shade loop to the runtime; Infinite Explorer Egg Mobile now
  composes yaw→bank→translation with public fixed-point matrices instead of
  rewriting every vertex in the game. The existing draw/VDP1 pipeline is
  unchanged for the pig, avoiding an unmeasured performance regression.
- Host box tests check facing sides, top visibility, material assignment,
  invalid extents/overflow and command exhaustion; animated instance tests
  check non-accumulating transforms, unchanged indices and atomic preflight.
  No emulator capture is implied by host/cross-build success.
- **Still open:** a shared camera-space **cross-model face planner** that
  merges world sections + pig + gems without converting the existing
  high-throughput projected pig path into hundreds of redundant per-face
  projections. Current `sat_scene3d_queue` sorts callbacks by one object
  anchor and cannot guarantee correct partial inter-occlusion. The eventual
  face planner needs vertex projection caches, per-face depth and visibility,
  bounded scratch and a stock-Saturn frame-time/command-budget gate before
  Skybridge uses it.
