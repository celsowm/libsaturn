# World LOD outdoor demo

This original outdoor scene exercises the shared `sat_world_lod3d_plan()`
planner with a generated castle facade, trees, a VDP2 ground plane, and a
movable camera focused on a stationary avatar. A host-only build step clips
the facade mesh into six spatial HLOD cells and generates three levels directly
from the same source geometry. The generated package has exact byte accounting,
fixed-point bounds checks, and per-cell quality metrics. At runtime it is
decoded once into fixed caller-owned mesh arrays; the generic scene painter
submits every selected mesh, keeping face ordering and VDP1 command emission
shared with other 3D consumers. LOD fade uses
`sat_world_lod3d_fade_apply()` and the existing `sat_fade3d_slot()` policy over
the VDP2 ground and sky.

## Controls

- D-pad Up/Down flies the camera target through the scene; the avatar stays
  fixed as a landmark.
- Left/Right changes camera yaw.
- L/R changes camera pitch.
- A/B changes camera distance.
- C toggles distance fade.
- Z temporarily marks each castle cell's finest mesh non-resident to exercise
  the planner's coarser-resident fallback; press X to reset the view and restore
  finest-level residency.
- Y toggles the optional budget governor.
- X resets the view and zoom, restores finest-level residency, and clears
  planner/fade history.

Every object, LOD mesh, planner buffer, scene buffer and temporary array has
fixed caller-owned storage. The scene has 23 logical objects: six generated
castle HLOD cells, sixteen trees, and the pinned avatar. It uses three generated
castle levels and tree levels, a 256-face budget, and a 384-command estimate
budget. Scenery degrades or drops before the protected avatar baseline.

The generated castle package is an original synthetic asset. Its binary and C
embedding are build outputs under `build/generated/world_lod3d_demo/`; no
generated media or private ROM assets are tracked in the repository. The build
report includes source checksum, source coverage, per-cell and per-level face
and vertex counts, texture estimates, fixed-camera silhouette comparisons,
geometric error, UV/material checks, and byte totals.

## Build and Ymir acceptance

Build with the POSIX workflow:

```sh
LIBSATURN_TOOLCHAIN_ROOT=/path/to/saturn-tools ./build-example.sh world_lod3d_demo
```

Run the camera route, screenshots, VDP2 trace, and telemetry checks:

```sh
LIBSATURN_BIOS=bios/firmware/mpr-17933.bin ./harness/run-world-lod3d-demo.sh
```

The BIOS is a local ignored input. The run script writes only under
`harness/build/world-lod3d-demo/`. The harness reports emulator field cadence;
it is not a stock Saturn timing claim. Telemetry lives in
`world_lod3d_demo.h` and can be read from a High WRAM dump with the map file.
