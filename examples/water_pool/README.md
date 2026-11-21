# Water Pool

Water Pool is a tiled pool with a floating ball and a live wave simulation,
after Evan Wallace's WebGL Water. It runs on a stock Sega Saturn at 320×224
through the SDK scene painter. The simulation is `saturn/water_sim.h`; the
ball is cut at the waterline with `sat_water_split_quad_shaded`.

| Saturn pad | Action |
| --- | --- |
| D-pad | Move the ball |
| X + D-pad | Move the ripple cursor |
| A | Drop a ripple at the cursor (hold for a trail) |
| B | Toggle gravity |
| Z + Up / Down | Raise or push the ball down |
| L / R | Orbit the camera |
| C | Cycle the 9×9, 13×13 and 17×17 grids |
| Y | Pause |
| Z + Y | Replay one ripple from a fresh pool, then pause at a fixed tick |
| Start | Reset |

The water is drawn the way Saturn games drew it: the surface and the water's
side facing the camera are VDP1 mesh, which covers alternating pixels and
blends on a composite display. Half-transparency would be a true 50/50 blend,
but a distorted VDP1 quad plots some pixels twice, so a translucent wave mesh
shows a double-blended hatch. What the original shows through the water
comes from the floor instead. Each floor node moves against the surface slope
above it, as refraction would shift it, so the tile grout bends with the
waves. The floor and walls take the surface light and caustics from the
water's curvature, so light patches move across the tiles. All of this is
per-vertex Gouraud shading, not per-pixel.

Painter passes keep the layering exact. The floor, the walls and the
submerged part of the ball are drawn first, then the water, then the dry part
of the ball. Only the ball faces that cross the water are cut; the dry and wet
faces go out as two instances, and back faces are skipped.

With the 9×9 grid, Ymir measured about 21 ms of scene preparation; the 17×17
grid takes about 50 ms. These are emulator figures only, and physical-hardware
acceptance is pending.

```sh
./build-example.sh water_pool
./run-example.sh water_pool
./harness/run-water-pool.sh
python3 harness/tests/test_water_pool.py
```

The procedural tile and ball are original and covered by the repository's MIT
license. The default local disc embeds a boot payload whose redistribution
rights are pending in the SDK packaging documentation.

[Water rendering](../../docs/WATER_RENDERING.md) · [Water Lab](../water_lab/README.md) · [License](../../LICENSE)
