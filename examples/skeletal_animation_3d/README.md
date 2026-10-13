# skeletal_animation_3d - Phase-8A MVP, memory-blocked

## What this example is

A rigged humanoid driven by a single animated clip on the new
`sat_skeleton` / `sat_skin` / `sat_skel_clip_player` runtime path.
Sister to the baked-pose `examples/basic_3d_animation` example, this
one runs the CPU-skinning pipeline instead of the static frame buffer
the baked-pose path produces. It is the next installable demo after
Phase 8A of `docs/SKELETAL_ANIMATION_PLAN.md`.

The asset is the rigged **Robot Police Unit (animated)** donated to
the repo under `assets/` (CC BY 4.0 by Jungle Jim, skfb.ly/pMLX7, kept
unmodified, SHA-256 `09085C8EB2A386E4259D5F3C8726CCD5D218E5147BCB486216736CE857C511AF`).
The importer is `tools/import_model.py --target skeletal`, driven by
the Makefile; the generated C/H pair lands in
`build/generated/skeletal_animation_3d/` and is committed for
reproducibility. The plan calls for six named clips (idle, walk, run,
jump, punch, kick); the free rigged asset ships with one looping
gesture (`3903460652032_TempMotion`, ~84 frames at 60 fps), so the
MVP drives one clip and documents the gap to the full spec.

## What this example is NOT (right now)

The asset's full mesh and skin tables together with the runtime's
per-frame deform pool exceed Saturn's 1 MB WRAMH even after moving
the working buffers to the 1 MB WRAM-L bank via
`__attribute__((section(".wram_l")))`. The linker overflows WRAMH
by ~180 KB and WRAM-L by ~160 KB, regardless of how the buffers are
arranged.

The committed `main.c` therefore ships a stub loop that builds,
boots to a black frame, and the full MVP body is gated behind
`-DSAT_SCN_FOR_REAL` for a future decimation pass. The
runtime path itself is verified by `tests/host/test_skeleton3d.cpp`
and `tests/host/test_skin3d.cpp`; the import path is verified by
`tests/tools/test_skeletal_export.py` running against
`chain_model` and the actual asset's import in this directory.

The intended-but-not-yet-shipping full MVP has the SAT_SCN_FOR_REAL
gate removed: it would deform the full 28,073-vertex mesh every frame,
stride the visible face count to ~5,000 quads (FACE_STRIDE = 4) for
the VDP1 command budget, and render with a single solid material.
Its source is in `main.c` already.

## Unblocking

Closing Phase 8A needs one of the following:

* **Decimated variant of this asset.** A meshopt-style collapse from
  ~28 K vertices / ~20 K triangles to ~5 K-8 K vertices keeps the
  rigged silhouette convincing while bringing the ROM + working set
  under the 1 MB cap. The import CLI already has `--max-triangles`;
  the same knob wired into the skeletal path drops the asset to a
  fitting size in a few seconds.

* **A smaller original rigged asset.** The original Phase-8A intent
  was a ~1,000-triangle demo character with one looping clip. A
  Mixamo-style humanoid with ~2 K vertices (instead of 28 K) would
  fit comfortably with the working set in WRAM-L and leave headroom
  for material/texture work the MVP currently skips.

Once either lands, removing `-DSAT_SCN_FOR_REAL` (or deleting the
gate entirely) lights the example up. The plumbing (CLI, asset,
runtime, host tests) is already there.

## Verified pieces

* `python -m unittest tests.tools.test_skeletal_export` - 9 tests pass.
* `make test` - all host tests green.
* `make EXAMPLE=skeletal_animation_3d` - imports + links + produces a
  loadable CD image (stub-mode).

## Files

* `assets/robot_police_unit_animated.glb` - the original asset.
* `assets/ROBOT_POLICE_ATTRIBUTION.md` - CC BY 4.0 attribution and
  SHA-256.
* `gen/robot_police_skeletal.h` and `gen/robot_police_skeletal.c` -
  the generated C/H pair from `tools/import_model.py --target skeletal`,
  committed so the build is reproducible offline. Regenerate locally
  by removing these and re-running the Makefile.
* `Makefile.inc` - the per-example build settings; uses the new
  `MODEL_TARGET=skeletal` knob in the Makefile.
* `main.c` - stub loop + full MVP body behind `-DSAT_SCN_FOR_REAL`.