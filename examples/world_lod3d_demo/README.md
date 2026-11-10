# World LOD outdoor demo

This original procedural scene exercises the shared `sat_world_lod3d_plan()`
planner with a castle-like tower group, trees, a ground plane, and a movable
avatar. The generic scene painter submits all selected geometry, so face
ordering and VDP1 command emission stay shared with other 3D consumers. LOD
fade uses `sat_world_lod3d_fade_apply()` and the existing `sat_fade3d_slot()`
policy over the VDP2 ground and sky.

## Controls

- D-pad Up/Down flies the camera target through the scene; the avatar stays
  fixed as a landmark.
- Left/Right changes camera yaw.
- L/R changes camera pitch.
- A/B changes camera distance.
- C toggles distance fade.
- Y toggles the optional budget governor.
- X resets the view and its planner/fade history.

Every object, LOD mesh, planner buffer, scene buffer and temporary array has
fixed caller-owned storage. The scene has 25 logical objects, three tower/tree
render levels, a 256-face budget and a 384-command estimate budget. The avatar
is pinned; scenery degrades or drops before that protected baseline.

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
