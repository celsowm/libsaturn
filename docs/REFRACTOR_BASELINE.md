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
