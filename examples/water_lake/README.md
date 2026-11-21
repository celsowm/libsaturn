# Water Lake

Water Lake is a 60×60-unit lake with shores, an island, a bridge on posts and a
deep bed, drawn two ways so the cost and the look of big-water support can be
compared on one camera route. It is the evidence for
[Big Water](../../docs/BIG_WATER_PLAN.md).

- **NAIVE** models the water as four large mesh quads and sorts the whole
  scene by depth, as a level would.
- **FULL** uses the SDK's big-water pieces: `saturn/water_plane.h` draws the
  surface as camera-centred rings of quads, `saturn/water_prepare.h` sorts the
  static level into faces above and below the surface (cutting the ones that
  cross it and tinting the wet ones) once at load, `saturn/water_actor.h` cuts
  the moving sphere at the waterline, `saturn/water_regions.h` answers where the
  water is, and `saturn/water_view.h` fades a VDP2 colour offset when the camera
  goes under. Painter passes keep the order exact: submerged geometry, the
  water, then everything above.

| Saturn pad | Action |
| --- | --- |
| D-pad | Move the sphere |
| L / R | Orbit the camera |
| X | Switch between FULL and NAIVE |
| Z + Y | Replay the 390-tick camera route in FULL mode |
| Z + X | Replay it in NAIVE mode |
| Z + A / Z + B | Replay it in FULL / NAIVE mode with textured water |
| C | Before a replay: count up the 30-tick section it pauses at (0: run to the end) |
| Y | Pause |
| Start | Reset |

A replay follows simulation ticks, ignores the pad and is identical in NTSC
and PAL. The route is a high orbit, an approach low over the shore, a skim, the
bridge, a dive under the surface and a rise. Telemetry (`water_lake_telemetry`)
records faces submitted, culled, clipped and fallback, world commands and the
preparation and flush time of every section, the time spent in each stage of
the last frame, and the ticks spent under water.

```sh
./build-example.sh water_lake
./run-example.sh water_lake
./harness/run-water-lake.sh
python3 harness/tests/test_water_lake.py
```

## What the measurements say

The results are in [Water benchmarks](../../docs/WATER_BENCHMARKS.md#water-lake)
and are Ymir figures, not hardware. FULL costs about 6–12 ms more preparation
per frame than NAIVE and does not reduce clipped or fallback faces, for flat or
for textured water: four large quads, or a grid of textured tiles, are handled
by the clip path, and a textured face behind the near plane is drawn in the
water colour instead of being dropped. The case for FULL is correctness, shown
by the pinned captures at ticks 120, 240 and 330: the bed no longer paints over
the water, the waterline cuts the sphere and the posts, wet geometry is tinted
by depth, and the camera under the surface gets the underwater fade.

The level, the sphere and the textures are original and covered by the
repository's MIT license. The default local disc embeds a boot payload whose
redistribution rights are pending in the SDK packaging documentation.

[Big Water plan](../../docs/BIG_WATER_PLAN.md) · [Water rendering](../../docs/WATER_RENDERING.md) · [Water Pool](../water_pool/README.md) · [License](../../LICENSE)
